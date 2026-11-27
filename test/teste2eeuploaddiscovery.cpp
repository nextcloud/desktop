/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientsideencryption.h"
#include "owncloudpropagator.h"
#include "syncenginetestutils.h"

#include <QJsonDocument>
#include <QJsonObject>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestE2eeUploadDiscovery : public QObject
{
    Q_OBJECT

    std::unique_ptr<FakeFolder> _folder;
    QSslCertificate _certificate;
    QByteArray _privateKey;
    const QByteArray _token = QByteArray("0123456789abcdef").repeated(4);

    void setApiVersion(double version)
    {
        _folder->account()->setCapabilities(
            {{u"end-to-end-encryption"_s, QVariantMap{{u"enabled"_s, true}, {u"api-version"_s, QString::number(version, 'f', 1)}}}});
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        auto certFile = QFile(u"e2etestsfakecert.pem"_s);
        QVERIFY(certFile.open(QIODevice::ReadOnly));
        _certificate = QSslCertificate(certFile.readAll());
        QVERIFY(!_certificate.isNull());
        auto keyFile = QFile(u"e2etestsfakecertprivatekey.pem"_s);
        QVERIFY(keyFile.open(QIODevice::ReadOnly));
        _privateKey = keyFile.readAll();
        QVERIFY(!_privateKey.isEmpty());
    }

    void init()
    {
        _folder = std::make_unique<FakeFolder>(FileInfo::A12_B12_C12_S12());
        _folder->account()->e2e()->setCertificate(_certificate);
        _folder->account()->e2e()->setPrivateKey(_privateKey);
        QVERIFY(QMetaObject::invokeMethod(_folder->account()->e2e(), "setMnemonic", Qt::DirectConnection, Q_ARG(QString, u"test mnemonic"_s)));
        setApiVersion(2.0);
    }

    void cleanup()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        _folder.reset();
    }

    void testUploadUnlockSchedulesDiscovery_data()
    {
        QTest::addColumn<int>("unlockStatus");
        QTest::newRow("committed") << 200;
        QTest::newRow("rejected") << 403;
    }

    void testUploadUnlockSchedulesDiscovery()
    {
        QFETCH(int, unlockStatus);
        QVERIFY(_folder->syncOnce());
        auto parent = SyncJournalFileRecord{};
        QVERIFY(_folder->syncJournal().getFileRecord(u"A"_s, &parent));
        parent._e2eEncryptionStatus = SyncJournalFileRecord::EncryptionStatus::EncryptedMigratedV2_0;
        QVERIFY(_folder->syncJournal().setFileRecord(parent));
        auto record = SyncJournalFileRecord{};
        QVERIFY(_folder->syncJournal().getFileRecord(u"A/a1"_s, &record));
        const auto item = SyncFileItem::fromSyncJournalFileRecord(record);
        item->_instruction = CSyncEnums::CSYNC_INSTRUCTION_SYNC;
        item->_direction = SyncFileItem::Up;
        auto lockRequests = 0;
        auto unlockRequests = 0;
        auto fileUploads = 0;
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) -> QNetworkReply * {
            const auto path = request.url().path();
            if (path.contains(u"/meta-data/"_s)) {
                if (op == QNetworkAccessManager::GetOperation) {
                    return new FakeErrorReply(op, request, _folder->networkAccessManager(), 404);
                }
                return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
            }
            if (path.contains(u"/lock/"_s)) {
                if (op == QNetworkAccessManager::DeleteOperation) {
                    ++unlockRequests;
                    if (unlockStatus == 200) {
                        return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
                    }
                    return new FakeErrorReply(op, request, _folder->networkAccessManager(), unlockStatus);
                }
                ++lockRequests;
                const auto body = QJsonDocument(QJsonObject{{u"ocs"_s, QJsonObject{{u"data"_s, QJsonObject{{u"e2e-token"_s, QString::fromLatin1(_token)}}}}}});
                return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200, body);
            }
            if (op == QNetworkAccessManager::PutOperation) {
                ++fileUploads;
                const auto reply = new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
                reply->setRawHeader("OC-FileID", record._fileId);
                reply->setRawHeader("ETag", "staged-etag");
                reply->setRawHeader("X-OC-MTime", "accepted");
                return reply;
            }
            return nullptr;
        });
        auto bulkUploadBlacklist = QSet<QString>{};
        auto propagator = OwncloudPropagator(_folder->account(), _folder->localPath(), u"/"_s, &_folder->syncJournal(), bulkUploadBlacklist);
        propagator.setSyncOptions(_folder->syncEngine().syncOptions());
        auto finished = QSignalSpy(&propagator, &OwncloudPropagator::finished);
        propagator.start(SyncFileItemVector{item});
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(lockRequests, 1);
        QCOMPARE(fileUploads, 1);
        QCOMPARE(unlockRequests, 1);
        QCOMPARE(propagator._anotherSyncNeeded, unlockStatus == 200);
        auto result = SyncJournalFileRecord{};
        QVERIFY(_folder->syncJournal().getFileRecord(u"A"_s, &result));
        QCOMPARE(result._etag, unlockStatus == 200 ? QByteArray("_invalid_") : parent._etag);
    }
};

QTEST_GUILESS_MAIN(TestE2eeUploadDiscovery)
#include "teste2eeuploaddiscovery.moc"
