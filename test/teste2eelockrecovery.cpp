/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include "common/ownsql.h"
#include "owncloudpropagator.h"
#include "syncenginetestutils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestE2eeLockRecovery : public QObject
{
    Q_OBJECT

    std::unique_ptr<FakeFolder> _folder;
    QSslCertificate _certificate;
    QByteArray _privateKey;
    const QByteArray _folderId = "43610";
    const QByteArray _token = QByteArray("0123456789abcdef").repeated(4);

    void setApiVersion(double version)
    {
        _folder->account()->setCapabilities(
            {{u"end-to-end-encryption"_s, QVariantMap{{u"enabled"_s, true}, {u"api-version"_s, QString::number(version, 'f', 1)}}}});
    }

    QByteArray saveToken(const QByteArray &folderId)
    {
        const auto e2ee = _folder->account()->e2e();
        const auto encrypted = EncryptionHelper::encryptStringAsymmetric(e2ee->getCertificateInformation(), e2ee->paddingMode(), *e2ee, _token);
        if (!encrypted) {
            return {};
        }
        _folder->syncJournal().setE2EeLockedFolder(folderId, *encrypted);
        return *encrypted;
    }

    LockEncryptFolderApiJob *newLockJob(QObject *parent)
    {
        const auto account = _folder->account();
        return new LockEncryptFolderApiJob(account,
                                           _folderId,
                                           account->e2e()->certificateSha256Fingerprint(),
                                           &_folder->syncJournal(),
                                           account->e2e()->getPublicKey(),
                                           parent);
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

    void testUnlockKeepsTokenUntilConfirmedSuccess_data()
    {
        QTest::addColumn<double>("apiVersion");
        QTest::addColumn<bool>("rollback");
        QTest::addColumn<int>("status");
        QTest::addColumn<int>("networkError");
        for (const auto apiVersion : {1.0, 2.0}) {
            for (const auto rollback : {false, true}) {
                for (const auto status : {200, 403, 404, 500}) {
                    const auto name = u"v%1-rollback%2-http%3"_s.arg(apiVersion).arg(rollback).arg(status).toLatin1();
                    QTest::newRow(name.constData()) << apiVersion << rollback << status
                                                    << int(status == 200 ? QNetworkReply::NoError : QNetworkReply::InternalServerError);
                }
                for (const auto error : {QNetworkReply::TimeoutError, QNetworkReply::OperationCanceledError}) {
                    for (const auto status : {0, 200}) {
                        const auto name = u"v%1-rollback%2-http%3-network%4"_s.arg(apiVersion).arg(rollback).arg(status).arg(int(error)).toLatin1();
                        QTest::newRow(name.constData()) << apiVersion << rollback << status << int(error);
                    }
                }
            }
        }
    }

    void testUnlockKeepsTokenUntilConfirmedSuccess()
    {
        QFETCH(double, apiVersion);
        QFETCH(bool, rollback);
        QFETCH(int, status);
        QFETCH(int, networkError);
        setApiVersion(apiVersion);
        const auto encrypted = saveToken(_folderId);
        QVERIFY(!encrypted.isEmpty());

        auto requests = QList<QNetworkRequest>{};
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) {
            requests.append(request);
            auto reply = new FakeErrorReply(op, request, _folder->networkAccessManager(), status);
            reply->setError(static_cast<QNetworkReply::NetworkError>(networkError), u"Synthetic unlock response"_s);
            return reply;
        });
        const auto job = new UnlockEncryptFolderApiJob(_folder->account(), _folderId, _token, &_folder->syncJournal(), this);
        job->setShouldRollbackMetadataChanges(rollback);
        auto success = QSignalSpy(job, &UnlockEncryptFolderApiJob::success);
        auto error = QSignalSpy(job, &UnlockEncryptFolderApiJob::error);
        auto done = QSignalSpy(job, &UnlockEncryptFolderApiJob::done);
        auto guardedJob = QPointer(job);
        job->start();
        QTRY_COMPARE(done.size(), 1);
        const auto succeeded = status == 200 && networkError == QNetworkReply::NoError;
        QCOMPARE(success.size(), succeeded ? 1 : 0);
        QCOMPARE(error.size(), succeeded ? 0 : 1);
        if (!succeeded) {
            QVERIFY(error.first().at(1).toInt() != 200);
        }
        QCOMPARE(_folder->syncJournal().e2EeLockedFolder(_folderId), succeeded ? QByteArray{} : encrypted);
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.first().rawHeader("e2e-token"), _token);
        QCOMPARE(QUrlQuery(requests.first().url()).queryItemValue(u"abort"_s), rollback ? u"true"_s : QString{});
        QVERIFY(requests.first().url().path().contains(u"/api/v%1/lock/"_s.arg(int(apiVersion))));
        QTRY_VERIFY(guardedJob.isNull());
    }

    void testRelockRecovery_data()
    {
        QTest::addColumn<double>("apiVersion");
        QTest::addColumn<int>("unlockStatus");
        QTest::addColumn<int>("lockStatus");
        for (const auto apiVersion : {1.0, 2.0}) {
            for (const auto status : {200, 403, 500}) {
                const auto name = u"v%1-unlock%2"_s.arg(apiVersion).arg(status).toLatin1();
                QTest::newRow(name.constData()) << apiVersion << status << 200;
            }
            const auto name = u"v%1-relock-failed"_s.arg(apiVersion).toLatin1();
            QTest::newRow(name.constData()) << apiVersion << 200 << 423;
        }
    }

    void testRelockRecovery()
    {
        QFETCH(double, apiVersion);
        QFETCH(int, unlockStatus);
        QFETCH(int, lockStatus);
        setApiVersion(apiVersion);
        const auto encrypted = saveToken(_folderId);
        QVERIFY(!encrypted.isEmpty());
        const auto newToken = QByteArray("new-lock-token");
        auto requests = QList<QNetworkRequest>{};
        auto operations = QList<QNetworkAccessManager::Operation>{};
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) -> QNetworkReply * {
            requests.append(request);
            operations.append(op);
            const auto status = op == QNetworkAccessManager::DeleteOperation ? unlockStatus : lockStatus;
            if (status != 200) {
                return new FakeErrorReply(op, request, _folder->networkAccessManager(), status);
            }
            const auto body = QJsonDocument(QJsonObject{{u"ocs"_s, QJsonObject{{u"data"_s, QJsonObject{{u"e2e-token"_s, QString::fromLatin1(newToken)}}}}}});
            return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200, body);
        });
        const auto job = newLockJob(this);
        job->setCounter(7);
        auto success = QSignalSpy(job, &LockEncryptFolderApiJob::success);
        auto error = QSignalSpy(job, &LockEncryptFolderApiJob::error);
        auto guardedJob = QPointer(job);
        job->start();
        QTRY_COMPARE(success.size() + error.size(), 1);
        QTRY_VERIFY(guardedJob.isNull());
        QCOMPARE(requests.size(), unlockStatus == 200 ? 2 : 1);
        QCOMPARE(operations.first(), QNetworkAccessManager::DeleteOperation);
        QCOMPARE(requests.first().rawHeader("e2e-token"), _token);
        QCOMPARE(QUrlQuery(requests.first().url()).queryItemValue(u"abort"_s), u"true"_s);
        if (unlockStatus != 200) {
            QCOMPARE(error.size(), 1);
            QCOMPARE(_folder->syncJournal().e2EeLockedFolder(_folderId), encrypted);
        } else {
            QCOMPARE(operations.last(), QNetworkAccessManager::PostOperation);
            QCOMPARE(requests.last().rawHeader("X-NC-E2EE-COUNTER"), apiVersion >= 2.0 ? QByteArray("7") : QByteArray{});
            if (lockStatus == 200) {
                QCOMPARE(success.size(), 1);
                QCOMPARE(success.first().at(1).toByteArray(), newToken);
                const auto e2ee = _folder->account()->e2e();
                const auto recovered = EncryptionHelper::decryptStringAsymmetric(e2ee->getCertificateInformation(),
                                                                                 e2ee->paddingMode(),
                                                                                 *e2ee,
                                                                                 _folder->syncJournal().e2EeLockedFolder(_folderId));
                QVERIFY(recovered);
                QCOMPARE(QByteArray::fromBase64(*recovered), newToken);
            } else {
                QCOMPARE(error.size(), 1);
                QVERIFY(_folder->syncJournal().e2EeLockedFolder(_folderId).isEmpty());
            }
        }
    }

    void testCorruptTokenTerminatesWithoutRequest()
    {
        const auto corruptToken = QByteArray("not-an-encrypted-token");
        _folder->syncJournal().setE2EeLockedFolder(_folderId, corruptToken);
        auto requests = 0;
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) {
            ++requests;
            return new FakeErrorReply(op, request, _folder->networkAccessManager(), 500);
        });
        const auto job = newLockJob(this);
        auto error = QSignalSpy(job, &LockEncryptFolderApiJob::error);
        auto guardedJob = QPointer(job);
        job->start();
        QTRY_COMPARE(error.size(), 1);
        QTRY_VERIFY(guardedJob.isNull());
        auto finished = QSignalSpy(&_folder->syncEngine(), &SyncEngine::finished);
        _folder->syncEngine().startSync();
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!finished.first().first().toBool());
        QVERIFY(!_folder->syncEngine().isSyncRunning());
        QCOMPARE(requests, 0);
        QCOMPARE(_folder->syncJournal().e2EeLockedFolder(_folderId), corruptToken);
    }

    void testLocalDeleteFailureDoesNotRelock()
    {
        const auto encrypted = saveToken(_folderId);
        QVERIFY(!encrypted.isEmpty());
        auto &journal = _folder->syncJournal();
        journal.close();
        {
            auto database = SqlDatabase{};
            QVERIFY(database.openOrCreateReadWrite(journal.databaseFilePath()));
            auto query = SqlQuery(database);
            query.prepare(
                "CREATE TRIGGER prevent_e2ee_lock_delete BEFORE DELETE ON e2EeLockedFolders "
                "BEGIN SELECT RAISE(FAIL, 'synthetic delete failure'); END;");
            QVERIFY(query.exec());
        }
        QVERIFY(journal.open());
        auto requests = 0;
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) {
            ++requests;
            return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
        });
        const auto job = newLockJob(this);
        auto error = QSignalSpy(job, &LockEncryptFolderApiJob::error);
        auto guardedJob = QPointer(job);
        job->start();
        QTRY_COMPARE(error.size(), 1);
        QTRY_VERIFY(guardedJob.isNull());
        QCOMPARE(requests, 1);
        QCOMPARE(journal.e2EeLockedFolder(_folderId), encrypted);
    }

    void testSyncWaitsForRecovery_data()
    {
        QTest::addColumn<double>("apiVersion");
        QTest::addColumn<int>("unlockStatus");
        QTest::addColumn<int>("failureOnAttempt");
        for (const auto apiVersion : {1.0, 2.0}) {
            for (const auto status : {200, 403, 500}) {
                for (const auto attempt : {1, 2}) {
                    const auto name = u"v%1-http%2-attempt%3"_s.arg(apiVersion).arg(status).arg(attempt).toLatin1();
                    QTest::newRow(name.constData()) << apiVersion << status << attempt;
                }
            }
        }
    }

    void testSyncWaitsForRecovery()
    {
        QFETCH(double, apiVersion);
        QFETCH(int, unlockStatus);
        QFETCH(int, failureOnAttempt);
        setApiVersion(apiVersion);
        const auto encrypted = saveToken(_folderId);
        QVERIFY(!encrypted.isEmpty());
        QVERIFY(!saveToken("43611").isEmpty());
        auto unlockRequests = QList<QNetworkRequest>{};
        auto discoveryBeforeRecovery = false;
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().contains(u"/lock/"_s)) {
                unlockRequests.append(request);
                if (unlockStatus != 200 && unlockRequests.size() >= failureOnAttempt) {
                    return new FakeErrorReply(op, request, _folder->networkAccessManager(), unlockStatus);
                }
                return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
            }
            discoveryBeforeRecovery |= !_folder->syncJournal().e2EeLockedFolders().isEmpty();
            return nullptr;
        });
        auto finished = QSignalSpy(&_folder->syncEngine(), &SyncEngine::finished);
        _folder->syncEngine().startSync();
        QVERIFY(_folder->syncEngine().isSyncRunning());
        QCOMPARE(unlockRequests.size(), 1);
        _folder->syncEngine().startSync();
        QCOMPARE(unlockRequests.size(), 1);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), unlockStatus == 200);
        QVERIFY(!_folder->syncEngine().isSyncRunning());
        QCOMPARE(unlockRequests.size(), unlockStatus == 200 ? 2 : failureOnAttempt);
        QVERIFY(!discoveryBeforeRecovery);
        for (const auto &request : std::as_const(unlockRequests)) {
            QCOMPARE(request.rawHeader("e2e-token"), _token);
            QCOMPARE(QUrlQuery(request.url()).queryItemValue(u"abort"_s), u"true"_s);
        }
        if (unlockStatus == 200) {
            QVERIFY(_folder->syncJournal().e2EeLockedFolders().isEmpty());
        } else {
            QCOMPARE(_folder->syncJournal().e2EeLockedFolders().size(), 3 - failureOnAttempt);
            // A later sync makes one new attempt, rather than looping in the failed job.
            _folder->syncEngine().startSync();
            QTRY_COMPARE(finished.size(), 2);
            QCOMPARE(unlockRequests.size(), failureOnAttempt + 1);
        }
    }

    void testCancelRecoveryThenSyncAgain()
    {
        const auto encrypted = saveToken(_folderId);
        QVERIFY(!encrypted.isEmpty());
        auto pendingReply = QPointer<FakeHangingReply>{};
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) -> QNetworkReply * {
            if (!request.url().path().contains(u"/lock/"_s)) {
                return nullptr;
            }
            auto pendingRequest = request;
            pendingRequest.setTransferTimeout(0);
            pendingReply = new FakeHangingReply(op, pendingRequest, _folder->networkAccessManager());
            return pendingReply;
        });
        auto finished = QSignalSpy(&_folder->syncEngine(), &SyncEngine::finished);
        _folder->syncEngine().startSync();
        QVERIFY(!pendingReply.isNull());
        _folder->syncEngine().abort();
        QCOMPARE(finished.size(), 1);
        QVERIFY(!finished.first().first().toBool());
        QVERIFY(!_folder->syncEngine().isSyncRunning());
        QTRY_VERIFY(pendingReply.isNull());
        QCOMPARE(_folder->syncJournal().e2EeLockedFolder(_folderId), encrypted);

        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().contains(u"/lock/"_s)) {
                return new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
            }
            return nullptr;
        });
        _folder->syncEngine().startSync();
        QTRY_COMPARE(finished.size(), 2);
        QVERIFY(finished.last().first().toBool());
        QVERIFY(_folder->syncJournal().e2EeLockedFolders().isEmpty());
    }

    void testCancelledRecoveryIgnoresLateSuccess()
    {
        const auto encrypted = saveToken(_folderId);
        QVERIFY(!encrypted.isEmpty());
        auto pendingReply = QPointer<FakeJsonReply>{};
        _folder->setServerOverride([&](auto op, const auto &request, QIODevice *) {
            pendingReply = new FakeJsonReply(op, request, _folder->networkAccessManager(), 200);
            return pendingReply.data();
        });
        auto finished = QSignalSpy(&_folder->syncEngine(), &SyncEngine::finished);
        _folder->syncEngine().startSync();
        QVERIFY(!pendingReply.isNull());
        _folder->syncEngine().abort();
        // Deliver a success before deleteLater() disposes of the cancelled job.
        pendingReply->slotSetFinished();
        QCOMPARE(finished.size(), 1);
        QVERIFY(!finished.first().first().toBool());
        QCOMPARE(_folder->syncJournal().e2EeLockedFolder(_folderId), encrypted);
        QTRY_VERIFY(pendingReply.isNull());
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
        QCOMPARE(_folder->syncJournal().e2EeLockedFolders().size(), unlockStatus == 200 ? 0 : 1);
    }
};

QTEST_GUILESS_MAIN(TestE2eeLockRecovery)
#include "teste2eelockrecovery.moc"
