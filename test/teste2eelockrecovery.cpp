/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include "common/ownsql.h"
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
};

QTEST_GUILESS_MAIN(TestE2eeLockRecovery)
#include "teste2eelockrecovery.moc"
