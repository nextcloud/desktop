/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "assistant/assistantattachment.h"
#include "assistant/assistantimageprovider.h"
#include "networkjobs.h"
#include "syncenginetestutils.h"

#include <QBuffer>
#include <QImage>
#include <QJsonObject>
#include <QScopedValueRollback>
#include <QUrlQuery>
#include <QtTest>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestAssistantAttachments : public QObject
{
    Q_OBJECT

private:
    AccountPtr _account;
    FakeQNAM *_network = nullptr;
    QList<QNetworkRequest> _requests;

    static QJsonDocument fileInfo(const QString &name)
    {
        return QJsonDocument{QJsonObject{{u"ocs"_s,
                                          QJsonObject{
                                              {u"meta"_s, QJsonObject{{u"statuscode"_s, 100}}},
                                              {u"data"_s, QJsonObject{{u"name"_s, name}}},
                                          }}}};
    }

    static QByteArray preview()
    {
        auto image = QImage{QSize{2, 2}, QImage::Format_RGB32};
        image.fill(Qt::red);
        auto buffer = QBuffer{};
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return buffer.data();
    }

private Q_SLOTS:
    void init()
    {
        _requests.clear();
        _account = Account::create();
        _account->setUrl(QUrl(u"https://cloud.example/nextcloud"_s));
        _network = new FakeQNAM({});
        _account->setCredentials(new FakeCredentials{_network});
    }

    void cleanup()
    {
        _account.reset();
    }

    void providerImagesAreIsolatedAndScaled()
    {
        auto image = QImage{QSize{40, 20}, QImage::Format_RGB32};
        image.fill(Qt::red);
        const auto source = AssistantImageProvider::insert(image);
        const auto otherSource = AssistantImageProvider::insert(image);
        QVERIFY(source != otherSource);
        const auto id = QUrl(source).path().mid(1);
        auto provider = AssistantImageProvider{};
        const auto scaled = provider.requestImage(id, nullptr, QSize{10, 10});
        QCOMPARE(scaled.size(), QSize(10, 5));
        QCOMPARE(scaled.pixelColor(0, 0), QColor(Qt::red));
        AssistantImageProvider::remove(source);
        QVERIFY(provider.requestImage(id, nullptr, {}).isNull());
        QVERIFY(!provider.requestImage(QUrl(otherSource).path().mid(1), nullptr, {}).isNull());
        AssistantImageProvider::remove(otherSource);
    }

    void storedFileUsesMetadataAndAuthenticatedPreview()
    {
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            _requests.append(request);
            if (request.url().path().endsWith("/info"_L1)) {
                return new FakePayloadReply(operation, request, fileInfo(u"Report.pdf"_s).toJson(QJsonDocument::Compact), _network);
            }
            return new FakePayloadReply(operation, request, preview(), _network);
        });
        auto attachment = AssistantAttachment{_account, 12, 42};
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QCOMPARE(attachment.name(), u"Report.pdf"_s);
        QVERIFY(!attachment.previewFailed());
        QVERIFY(attachment.previewSource().startsWith("image://assistant-attachments/"_L1));
        auto provider = AssistantImageProvider{};
        const auto imageId = QUrl(attachment.previewSource()).path().mid(1);
        const auto image = provider.requestImage(imageId, nullptr, {});
        QVERIFY(!image.isNull());
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(attachment.openUrl(), QUrl(u"https://cloud.example/nextcloud/index.php/f/12"_s));
        QVERIFY(attachment.downloadUrl().path().endsWith("/file/12/display"_L1));
        QCOMPARE(_requests.size(), 2);
        QCOMPARE(_requests[0].url().path(), u"/nextcloud/ocs/v2.php/apps/assistant/api/v1/file/12/info"_s);
        QCOMPARE(_requests[1].url().path(), u"/nextcloud/index.php/core/preview"_s);
        QCOMPARE(QUrlQuery(_requests[1].url()).queryItemValue(u"fileId"_s), u"12"_s);
        QCOMPARE(QUrlQuery(_requests[1].url()).queryItemValue(u"x"_s), u"320"_s);
        for (const auto &request : _requests) {
            QCOMPARE(request.rawHeader("OCS-APIREQUEST"), QByteArray("true"));
        }
        attachment.cancel();
        QVERIFY(provider.requestImage(imageId, nullptr, {}).isNull());
    }

    void destroyingAttachmentReleasesPreview()
    {
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().endsWith("/info"_L1)) {
                return new FakePayloadReply(operation, request, fileInfo(u"Picture.png"_s).toJson(QJsonDocument::Compact), _network);
            }
            return new FakePayloadReply(operation, request, preview(), _network);
        });
        const auto attachment = new AssistantAttachment(_account, 12, -1);
        attachment->load();
        QTRY_VERIFY(!attachment->loading());
        const auto id = QUrl(attachment->previewSource()).path().mid(1);
        auto provider = AssistantImageProvider{};
        QVERIFY(!provider.requestImage(id, nullptr, {}).isNull());
        delete attachment;
        QVERIFY(provider.requestImage(id, nullptr, {}).isNull());
    }

    void taskOutputFallsBackToItsOriginatingTask()
    {
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            _requests.append(request);
            if (request.url().path().endsWith("/info"_L1)) {
                return new FakeErrorReply(operation, request, _network, 404);
            }
            return new FakePayloadReply(operation, request, preview(), _network);
        });
        auto attachment = AssistantAttachment{_account, 12, 43};
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QVERIFY(!attachment.previewFailed());
        QVERIFY(attachment.openUrl().isEmpty());
        QVERIFY(attachment.downloadUrl().path().endsWith("/task/43/output-file/12/download"_L1));
        QCOMPARE(_requests.size(), 2);
        QVERIFY(_requests[1].url().path().endsWith("/task/43/output-file/12/preview"_L1));
    }

    void unavailablePreviewsKeepTheFileAction_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("unsupported") << QByteArray("not an image");
        QTest::newRow("empty") << QByteArray{};
        QTest::newRow("oversized") << QByteArray(4 * 1024 * 1024 + 1, 'x');
    }

    void unavailablePreviewsKeepTheFileAction()
    {
        QFETCH(QByteArray, body);
        _network->setOverride([this, body](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().endsWith("/info"_L1)) {
                return new FakePayloadReply(operation, request, fileInfo(u"Archive.zip"_s).toJson(QJsonDocument::Compact), _network);
            }
            return new FakePayloadReply(operation, request, body, _network);
        });
        auto attachment = AssistantAttachment{_account, 12, -1};
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QVERIFY(attachment.previewFailed());
        QVERIFY(attachment.previewSource().isEmpty());
        QVERIFY(!attachment.openUrl().isEmpty());
        QVERIFY(!attachment.downloadUrl().isEmpty());
    }

    void permissionFailureDoesNotTryTaskOutput()
    {
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            _requests.append(request);
            return new FakeErrorReply(operation, request, _network, 403);
        });
        auto attachment = AssistantAttachment{_account, 12, 42};
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QVERIFY(attachment.previewFailed());
        QCOMPARE(_requests.size(), 1);
    }

    void cancellationAndRetryIgnorePreviousReplies()
    {
        auto firstReply = QPointer<QNetworkReply>{};
        _network->setOverride([this, &firstReply](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            firstReply = new FakeHangingReply(operation, request, _network);
            return firstReply;
        });
        auto attachment = AssistantAttachment{_account, 12, -1};
        attachment.load();
        QVERIFY(attachment.loading());
        attachment.cancel();
        QVERIFY(!attachment.loading());
        QVERIFY(attachment.previewSource().isEmpty());
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().endsWith("/info"_L1)) {
                return new FakePayloadReply(operation, request, fileInfo(u"Retried.png"_s).toJson(QJsonDocument::Compact), _network);
            }
            return new FakePayloadReply(operation, request, preview(), _network);
        });
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QTRY_VERIFY(firstReply.isNull());
        QCOMPARE(attachment.name(), u"Retried.png"_s);
        QVERIFY(!attachment.previewFailed());
    }

    void timeoutLeavesTheLoadingState()
    {
        const auto timeoutGuard = QScopedValueRollback<bool>{AbstractNetworkJob::enableTimeout, true};
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            return new FakeHangingReply(operation, request, _network);
        });
        auto attachment = AssistantAttachment{_account, 12, -1};
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QVERIFY(attachment.previewFailed());
    }

    void accountsOnTheSameServerUseTheirOwnCredentials()
    {
        _network->setOverride([this](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            _requests.append(request);
            return new FakeErrorReply(operation, request, _network, 403);
        });
        const auto otherAccount = Account::create();
        otherAccount->setUrl(_account->url());
        const auto otherNetwork = new FakeQNAM({});
        otherAccount->setCredentials(new FakeCredentials{otherNetwork});
        otherNetwork->setOverride([otherNetwork](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().endsWith("/info"_L1)) {
                return new FakePayloadReply(operation, request, fileInfo(u"Other account.png"_s).toJson(QJsonDocument::Compact), otherNetwork);
            }
            return new FakePayloadReply(operation, request, preview(), otherNetwork);
        });
        auto attachment = AssistantAttachment{otherAccount, 12, -1};
        attachment.load();
        QTRY_VERIFY(!attachment.loading());
        QCOMPARE(attachment.name(), u"Other account.png"_s);
        QVERIFY(!attachment.previewFailed());
        QVERIFY(_requests.isEmpty());
    }

    void destructionCancelsPendingRequest()
    {
        auto reply = QPointer<QNetworkReply>{};
        _network->setOverride([this, &reply](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            reply = new FakeHangingReply(operation, request, _network);
            return reply;
        });
        const auto attachment = new AssistantAttachment(_account, 12, -1);
        attachment->load();
        QVERIFY(reply);
        delete attachment;
        QTRY_VERIFY(reply.isNull());
    }
};

QTEST_GUILESS_MAIN(TestAssistantAttachments)
#include "testassistantattachments.moc"
