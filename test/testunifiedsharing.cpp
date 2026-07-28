/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/sharing/createsharejob.h"
#include "gui/sharing/destroysharejob.h"
#include "gui/sharing/recipientsearchmodel.h"
#include "gui/sharing/searchrecipientsjob.h"
#include "gui/sharing/share.h"
#include "gui/sharing/unifiedsharingapi.h"
#include "gui/sharing/unifiedsharingrequest.h"
#include "gui/sharing/updatesharejob.h"
#include "syncenginetestutils.h"

#include <QSignalSpy>
#include <QTest>
#include <QUrlQuery>

using namespace OCC;
using namespace OCC::Gui::Sharing;
using namespace Qt::StringLiterals;

class TestUnifiedSharing : public QObject
{
    Q_OBJECT

private slots:
    void requestsAreConfiguredBeforeTheyStart()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        auto requestPath = QString{};
        auto requestVerb = QByteArray{};

        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            requestPath = request.url().path();
            requestVerb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            if (requestVerb.isEmpty()) {
                switch (operation) {
                case QNetworkAccessManager::GetOperation:
                    requestVerb = "GET";
                    break;
                case QNetworkAccessManager::PostOperation:
                    requestVerb = "POST";
                    break;
                case QNetworkAccessManager::PutOperation:
                    requestVerb = "PUT";
                    break;
                case QNetworkAccessManager::DeleteOperation:
                    requestVerb = "DELETE";
                    break;
                default:
                    break;
                }
            }

            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": 200,
                        "message": "OK"
                    },
                    "data": {
                        "id": "share-1"
                    }
                }
            })json",
                                        this};
        });

        UnifiedSharingApi api{fakeFolder.account()};
        const auto share = Share::fromJson(QJsonDocument::fromJson(R"json({"ocs":{"data":{"id":"share-1"}}})json"), fakeFolder.account());

        const auto verifyRequest = [&](UnifiedSharingRequest *job,
                                       const QByteArray &expectedVerb,
                                       const QString &expectedPath,
                                       const QHash<QString, QString> &expectedParameters = {}) {
            for (auto it = expectedParameters.cbegin(); it != expectedParameters.cend(); ++it) {
                QCOMPARE(job->getParamValue(it.key()), it.value());
            }

            QSignalSpy finishedSpy{job, &UnifiedSharingRequest::jobFinished};
            job->start();
            QVERIFY(finishedSpy.wait());
            QCOMPARE(requestVerb, expectedVerb);
            QVERIFY2(requestPath.endsWith(expectedPath), qPrintable(requestPath));
        };

        verifyRequest(api.createShare(), "POST", "/ocs/v2.php/apps/sharing/api/v1/share");
        verifyRequest(api.destroyShare("share-1"_L1), "DELETE", "/ocs/v2.php/apps/sharing/api/v1/share/share-1");
        verifyRequest(api.addSource(share, "42"_L1),
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/source",
                      {{"class"_L1, "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1}, {"value"_L1, "42"_L1}});
        verifyRequest(api.addRecipient(share, "recipient-class"_L1, "alice"_L1),
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/recipient",
                      {{"class"_L1, "recipient-class"_L1}, {"value"_L1, "alice"_L1}});
        verifyRequest(api.removeRecipient(share, "recipient-class"_L1, "alice"_L1),
                      "DELETE",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/recipient",
                      {{"class"_L1, "recipient-class"_L1}, {"value"_L1, "alice"_L1}});
        verifyRequest(api.searchRecipients("ali"_L1, 10, 20),
                      "GET",
                      "/ocs/v2.php/apps/sharing/api/v1/recipients",
                      {{"query"_L1, "ali"_L1}, {"offset"_L1, "10"_L1}, {"limit"_L1, "20"_L1}});
        verifyRequest(api.setPermission(share, "permission-class"_L1, true),
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/permission",
                      {{"class"_L1, "permission-class"_L1}, {"enabled"_L1, "true"_L1}});
        verifyRequest(api.setPermissionPreset(share, "preset-class"_L1),
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/permission/preset",
                      {{"permissionPreset"_L1, "preset-class"_L1}});

        QCOMPARE(requestCount, 8);
        delete share;
    }

    void requestStartsOnlyOnce()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": 200,
                        "message": "OK"
                    },
                    "data": {}
                }
            })json",
                                        this};
        });

        UnifiedSharingApi api{fakeFolder.account()};
        const auto job = api.searchRecipients("alice"_L1, 0, 10);
        QSignalSpy finishedSpy{job, &UnifiedSharingRequest::jobFinished};

        job->start();
        job->start();

        QVERIFY(finishedSpy.wait());
        QCOMPARE(requestCount, 1);
    }

    void typedJobsParseTheirResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto data =
                request.url().path().endsWith("/recipients"_L1) ? R"json([{"label":"Alice"}])json" : R"json({"id":"share-1","state":"active"})json";
            const auto statusCode = operation == QNetworkAccessManager::DeleteOperation ? 204 : 200;
            const auto payload = QString{R"json({"ocs":{"meta":{"status":"ok","statuscode":%1,"message":"OK"},"data":%2}})json"}
                                     .arg(statusCode)
                                     .arg(QString::fromUtf8(data))
                                     .toUtf8();
            return new FakePayloadReply{operation, request, payload, this};
        });

        UnifiedSharingApi api{fakeFolder.account()};

        QPointer<Share> createdShare;
        const auto createJob = api.createShare();
        connect(createJob, &CreateShareJob::shareCreated, this, [&](QPointer<Share> share) {
            createdShare = share;
        });
        createJob->start();
        QTRY_VERIFY(createdShare);
        QCOMPARE(createdShare->id(), "share-1"_L1);

        auto updateReceived = false;
        const auto updateJob = api.setPermission(createdShare, "permission-class"_L1, true);
        connect(updateJob, &UpdateShareJob::shareUpdated, this, [&](QPointer<Share> share) {
            updateReceived = share == createdShare;
        });
        updateJob->start();
        QTRY_VERIFY(updateReceived);
        QCOMPARE(createdShare->state(), Share::ShareState::Active);

        auto recipients = QJsonArray{};
        const auto searchJob = api.searchRecipients("ali"_L1, 0, 10);
        QCOMPARE(searchJob->timeoutMsec(), 10'000);
        connect(searchJob, &SearchRecipientsJob::recipientsFound, this, [&](const QJsonArray &result) {
            recipients = result;
        });
        searchJob->start();
        QTRY_COMPARE(recipients.size(), 1);
        QCOMPARE(recipients.at(0).toObject().value("label"_L1).toString(), "Alice"_L1);

        auto destroyed = false;
        const auto destroyJob = api.destroyShare(createdShare->id());
        connect(destroyJob, &DestroyShareJob::jobFinished, this, [&](const QJsonDocument &, int) {
            destroyed = true;
        });
        destroyJob->start();
        QTRY_VERIFY(destroyed);

        delete createdShare;
    }

    void recipientSearchIsDebouncedAndIgnoresStaleResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            const auto query = QUrlQuery{request.url()}.queryItemValue("query"_L1);
            const auto payload =
                QString{R"json({"ocs":{"meta":{"status":"ok","statuscode":200,"message":"OK"},"data":[{"display_name":"%1"}]}})json"}.arg(query).toUtf8();
            const auto delay = query == "old"_L1 ? 800 : FakePayloadReply::defaultDelay;
            return new FakePayloadReply{operation, request, payload, delay, this};
        });

        RecipientSearchModel model;
        model.setAccount(fakeFolder.account());
        model.setQuery("o"_L1);
        model.setQuery("ol"_L1);
        model.setQuery("old"_L1);

        QTest::qWait(200);
        QCOMPARE(requestCount, 0);
        QTRY_COMPARE_WITH_TIMEOUT(requestCount, 1, 500);

        model.setQuery("new"_L1);
        QTRY_COMPARE_WITH_TIMEOUT(requestCount, 2, 500);
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 500);
        QCOMPARE(model.data(model.index(0), RecipientSearchModel::DisplayNameRole).toString(), "new"_L1);

        QTest::qWait(500);
        QCOMPARE(model.data(model.index(0), RecipientSearchModel::DisplayNameRole).toString(), "new"_L1);
    }
};

QTEST_GUILESS_MAIN(TestUnifiedSharing)

#include "testunifiedsharing.moc"
