/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lockfilejobs.h"

#include "account.h"
#include "accountstate.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "syncenginetestutils.h"
#include "localdiscoverytracker.h"

#include <QTest>
#include <QSignalSpy>

using namespace Qt::StringLiterals;

class TestLockFile : public QObject
{
    Q_OBJECT

public:
    TestLockFile() = default;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testLockFile_lockFile_lockSuccess()
    {
        const auto testFileName = QStringLiteral("file.txt");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QSignalSpy lockFileSuccessSpy(fakeFolder.account().data(), &OCC::Account::lockFileSuccess);
        QSignalSpy lockFileErrorSpy(fakeFolder.account().data(), &OCC::Account::lockFileError);

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.account()->setLockFileState(QStringLiteral("/") + testFileName,
                                               QStringLiteral("/"),
                                               fakeFolder.localPath(),
                                               {},
                                               &fakeFolder.syncJournal(),
                                               OCC::SyncFileItem::LockStatus::LockedItem,
                                               OCC::SyncFileItem::LockOwnerType::UserLock);

        QVERIFY(lockFileSuccessSpy.wait());
        QCOMPARE(lockFileErrorSpy.count(), 0);
    }

    void testLockFile_lockFile_lockError()
    {
        const auto testFileName = QStringLiteral("file.txt");
        static constexpr auto LockedHttpErrorCode = 423;
        const auto replyData = QByteArray("<?xml version=\"1.0\"?>\n"
                                          "<d:prop xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
                                          " <nc:lock/>\n"
                                          " <nc:lock-owner-type>0</nc:lock-owner-type>\n"
                                          " <nc:lock-owner>john</nc:lock-owner>\n"
                                          " <nc:lock-owner-displayname>John Doe</nc:lock-owner-displayname>\n"
                                          " <nc:lock-owner-editor>john</nc:lock-owner-editor>\n"
                                          " <nc:lock-time>1650619678</nc:lock-time>\n"
                                          " <nc:lock-timeout>300</nc:lock-timeout>\n"
                                          " <nc:lock-token>files_lock/310997d7-0aae-4e48-97e1-eeb6be6e2202</nc:lock-token>\n"
                                          "</d:prop>\n");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([replyData] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, LockedHttpErrorCode, replyData);
            }

            return reply;
        });

        QSignalSpy lockFileSuccessSpy(fakeFolder.account().data(), &OCC::Account::lockFileSuccess);
        QSignalSpy lockFileErrorSpy(fakeFolder.account().data(), &OCC::Account::lockFileError);

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.account()->setLockFileState(QStringLiteral("/") + testFileName,
                                               QStringLiteral("/"),
                                               fakeFolder.localPath(),
                                               {},
                                               &fakeFolder.syncJournal(),
                                               OCC::SyncFileItem::LockStatus::LockedItem,
                                               OCC::SyncFileItem::LockOwnerType::UserLock);

        QVERIFY(lockFileErrorSpy.wait());
        QCOMPARE(lockFileSuccessSpy.count(), 0);
    }

    void testLockFile_fileLockStatus_queryLockStatus()
    {
        const auto testFileName = QStringLiteral("file.txt");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QSignalSpy lockFileSuccessSpy(fakeFolder.account().data(), &OCC::Account::lockFileSuccess);
        QSignalSpy lockFileErrorSpy(fakeFolder.account().data(), &OCC::Account::lockFileError);

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.account()->setLockFileState(QStringLiteral("/") + testFileName,
                                               QStringLiteral("/"),
                                               fakeFolder.localPath(),
                                               {},
                                               &fakeFolder.syncJournal(),
                                               OCC::SyncFileItem::LockStatus::LockedItem,
                                               OCC::SyncFileItem::LockOwnerType::UserLock);

        QVERIFY(lockFileSuccessSpy.wait());
        QCOMPARE(lockFileErrorSpy.count(), 0);

        auto lockStatus = fakeFolder.account()->fileLockStatus(&fakeFolder.syncJournal(), testFileName);
        QCOMPARE(lockStatus, OCC::SyncFileItem::LockStatus::LockedItem);
    }

    void testLockFile_fileCanBeUnlocked_canUnlock()
    {
        const auto testFileName = QStringLiteral("file.txt");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QSignalSpy lockFileSuccessSpy(fakeFolder.account().data(), &OCC::Account::lockFileSuccess);
        QSignalSpy lockFileErrorSpy(fakeFolder.account().data(), &OCC::Account::lockFileError);

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.account()->setLockFileState(QStringLiteral("/") + testFileName,
                                               QStringLiteral("/"),
                                               fakeFolder.localPath(),
                                               {},
                                               &fakeFolder.syncJournal(),
                                               OCC::SyncFileItem::LockStatus::LockedItem,
                                               OCC::SyncFileItem::LockOwnerType::UserLock);

        QVERIFY(lockFileSuccessSpy.wait());
        QCOMPARE(lockFileErrorSpy.count(), 0);

        auto lockStatus = fakeFolder.account()->fileCanBeUnlocked(&fakeFolder.syncJournal(), testFileName);
        QCOMPARE(lockStatus, true);
    }

    void testLockFile_lockFile_jobSuccess()
    {
        const auto testFileName = QStringLiteral("file.txt");
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + testFileName,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::LockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobSuccess.wait());
        QCOMPARE(jobFailure.count(), 0);

        auto fileRecord = OCC::SyncJournalFileRecord{};
        QVERIFY(fakeFolder.syncJournal().getFileRecord(testFileName, &fileRecord));
        QCOMPARE(fileRecord._lockstate._locked, true);
        QCOMPARE(fileRecord._lockstate._lockEditorApp, QString{});
        QCOMPARE(fileRecord._lockstate._lockOwnerDisplayName, QStringLiteral("John Doe"));
        QCOMPARE(fileRecord._lockstate._lockOwnerId, QStringLiteral("admin"));
        QCOMPARE(fileRecord._lockstate._lockOwnerType, static_cast<qint64>(OCC::SyncFileItem::LockOwnerType::UserLock));
        QCOMPARE(fileRecord._lockstate._lockTime, 1234560);
        QCOMPARE(fileRecord._lockstate._lockTimeout, 1800);

        QVERIFY(fakeFolder.syncOnce());
    }

    void testLockFile_lockFile_unlockFile_jobSuccess()
    {
        const auto testFileName = QStringLiteral("file.txt");
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto lockFileJob = new OCC::LockFileJob(fakeFolder.account(),
                                                &fakeFolder.syncJournal(),
                                                QStringLiteral("/") + testFileName,
                                                QStringLiteral("/"),
                                                fakeFolder.localPath(),
                                                {},
                                                OCC::SyncFileItem::LockStatus::LockedItem,
                                                OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy lockFileJobSuccess(lockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy lockFileJobFailure(lockFileJob, &OCC::LockFileJob::finishedWithError);

        lockFileJob->start();

        QVERIFY(lockFileJobSuccess.wait());
        QCOMPARE(lockFileJobFailure.count(), 0);

        QVERIFY(fakeFolder.syncOnce());

        auto unlockFileJob = new OCC::LockFileJob(fakeFolder.account(),
                                                  &fakeFolder.syncJournal(),
                                                  QStringLiteral("/") + testFileName,
                                                  QStringLiteral("/"),
                                                  fakeFolder.localPath(),
                                                  {},
                                                  OCC::SyncFileItem::LockStatus::UnlockedItem,
                                                  OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy unlockFileJobSuccess(unlockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy unlockFileJobFailure(unlockFileJob, &OCC::LockFileJob::finishedWithError);

        unlockFileJob->start();

        QVERIFY(unlockFileJobSuccess.wait());
        QCOMPARE(unlockFileJobFailure.count(), 0);

        auto fileRecord = OCC::SyncJournalFileRecord{};
        QVERIFY(fakeFolder.syncJournal().getFileRecord(testFileName, &fileRecord));
        QCOMPARE(fileRecord._lockstate._locked, false);

        QVERIFY(fakeFolder.syncOnce());
    }

    void testLockFile_lockFile_alreadyLockedByUser()
    {
        static constexpr auto LockedHttpErrorCode = 423;
        static constexpr auto PreconditionFailedHttpErrorCode = 412;

        const auto testFileName = QStringLiteral("file.txt");

        const auto replyData = QByteArray("<?xml version=\"1.0\"?>\n"
                                          "<d:prop xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
                                          " <nc:lock>1</nc:lock>\n"
                                          " <nc:lock-owner-type>0</nc:lock-owner-type>\n"
                                          " <nc:lock-owner>john</nc:lock-owner>\n"
                                          " <nc:lock-owner-displayname>John Doe</nc:lock-owner-displayname>\n"
                                          " <nc:lock-owner-editor>john</nc:lock-owner-editor>\n"
                                          " <nc:lock-time>1650619678</nc:lock-time>\n"
                                          " <nc:lock-timeout>300</nc:lock-timeout>\n"
                                          " <nc:lock-token>files_lock/310997d7-0aae-4e48-97e1-eeb6be6e2202</nc:lock-token>\n"
                                          "</d:prop>\n");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([replyData] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, LockedHttpErrorCode, replyData);
            } else if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, PreconditionFailedHttpErrorCode, replyData);
            }

            return reply;
        });

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + testFileName,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::LockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobFailure.wait());
        QCOMPARE(jobSuccess.count(), 0);
    }

    void testLockFile_lockFile_alreadyLockedByApp()
    {
        static constexpr auto LockedHttpErrorCode = 423;
        static constexpr auto PreconditionFailedHttpErrorCode = 412;

        const auto testFileName = QStringLiteral("file.txt");

        const auto replyData = QByteArray("<?xml version=\"1.0\"?>\n"
                                          "<d:prop xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
                                          " <nc:lock>1</nc:lock>\n"
                                          " <nc:lock-owner-type>1</nc:lock-owner-type>\n"
                                          " <nc:lock-owner>john</nc:lock-owner>\n"
                                          " <nc:lock-owner-displayname>John Doe</nc:lock-owner-displayname>\n"
                                          " <nc:lock-owner-editor>Text</nc:lock-owner-editor>\n"
                                          " <nc:lock-time>1650619678</nc:lock-time>\n"
                                          " <nc:lock-timeout>300</nc:lock-timeout>\n"
                                          " <nc:lock-token>files_lock/310997d7-0aae-4e48-97e1-eeb6be6e2202</nc:lock-token>\n"
                                          "</d:prop>\n");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([replyData] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, LockedHttpErrorCode, replyData);
            } else if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, PreconditionFailedHttpErrorCode, replyData);
            }

            return reply;
        });

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + testFileName,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::LockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobFailure.wait());
        QCOMPARE(jobSuccess.count(), 0);
    }

    void testLockFile_unlockFile_alreadyUnlocked()
    {
        static constexpr auto LockedHttpErrorCode = 423;
        static constexpr auto PreconditionFailedHttpErrorCode = 412;

        const auto testFileName = QStringLiteral("file.txt");

        const auto replyData = QByteArray("<?xml version=\"1.0\"?>\n"
                                          "<d:prop xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
                                          " <nc:lock/>\n"
                                          " <nc:lock-owner-type>0</nc:lock-owner-type>\n"
                                          " <nc:lock-owner>john</nc:lock-owner>\n"
                                          " <nc:lock-owner-displayname>John Doe</nc:lock-owner-displayname>\n"
                                          " <nc:lock-owner-editor>john</nc:lock-owner-editor>\n"
                                          " <nc:lock-time>1650619678</nc:lock-time>\n"
                                          " <nc:lock-timeout>300</nc:lock-timeout>\n"
                                          " <nc:lock-token>files_lock/310997d7-0aae-4e48-97e1-eeb6be6e2202</nc:lock-token>\n"
                                          "</d:prop>\n");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([replyData] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, LockedHttpErrorCode, replyData);
            } else if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, PreconditionFailedHttpErrorCode, replyData);
            }

            return reply;
        });

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + testFileName,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::UnlockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobSuccess.wait());
        QCOMPARE(jobFailure.count(), 0);
    }

    void testLockFile_unlockFile_lockedBySomeoneElse()
    {
        static constexpr auto LockedHttpErrorCode = 423;

        const auto testFileName = QStringLiteral("file.txt");

        const auto replyData = QByteArray("<?xml version=\"1.0\"?>\n"
                                          "<d:prop xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
                                          " <nc:lock>1</nc:lock>\n"
                                          " <nc:lock-owner-type>0</nc:lock-owner-type>\n"
                                          " <nc:lock-owner>alice</nc:lock-owner>\n"
                                          " <nc:lock-owner-displayname>Alice Doe</nc:lock-owner-displayname>\n"
                                          " <nc:lock-owner-editor>Text</nc:lock-owner-editor>\n"
                                          " <nc:lock-time>1650619678</nc:lock-time>\n"
                                          " <nc:lock-timeout>300</nc:lock-timeout>\n"
                                          " <nc:lock-token>files_lock/310997d7-0aae-4e48-97e1-eeb6be6e2202</nc:lock-token>\n"
                                          "</d:prop>\n");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([replyData] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK") ||
                                                                 request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK"))) {
                reply = new FakeErrorReply(op, request, nullptr, LockedHttpErrorCode, replyData);
            }

            return reply;
        });

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + testFileName,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::UnlockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobFailure.wait());
        QCOMPARE(jobSuccess.count(), 0);
    }

    void testLockFile_lockFile_jobError()
    {
        const auto testFileName = QStringLiteral("file.txt");
        static constexpr auto InternalServerErrorHttpErrorCode = 500;

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK") ||
                                                                 request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK"))) {
                reply = new FakeErrorReply(op, request, nullptr, InternalServerErrorHttpErrorCode, {});
            }

            return reply;
        });

        fakeFolder.localModifier().insert(QStringLiteral("file.txt"));

        QVERIFY(fakeFolder.syncOnce());

        auto lockFileJob = new OCC::LockFileJob(fakeFolder.account(),
                                                &fakeFolder.syncJournal(),
                                                QStringLiteral("/") + testFileName,
                                                QStringLiteral("/"),
                                                fakeFolder.localPath(),
                                                {},
                                                OCC::SyncFileItem::LockStatus::LockedItem,
                                                OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy lockFileJobSuccess(lockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy lockFileJobFailure(lockFileJob, &OCC::LockFileJob::finishedWithError);

        lockFileJob->start();

        QVERIFY(lockFileJobFailure.wait());
        QCOMPARE(lockFileJobSuccess.count(), 0);

        QVERIFY(fakeFolder.syncOnce());

        auto unlockFileJob = new OCC::LockFileJob(fakeFolder.account(),
                                                  &fakeFolder.syncJournal(),
                                                  QStringLiteral("/") + testFileName,
                                                  QStringLiteral("/"),
                                                  fakeFolder.localPath(),
                                                  {},
                                                  OCC::SyncFileItem::LockStatus::UnlockedItem,
                                                  OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy unlockFileJobSuccess(unlockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy unlockFileJobFailure(unlockFileJob, &OCC::LockFileJob::finishedWithError);

        unlockFileJob->start();

        QVERIFY(unlockFileJobFailure.wait());
        QCOMPARE(unlockFileJobSuccess.count(), 0);

        QVERIFY(fakeFolder.syncOnce());
    }

    void testLockFile_lockFile_preconditionFailedError()
    {
        static constexpr auto PreconditionFailedHttpErrorCode = 412;

        const auto testFileName = QStringLiteral("file.txt");

        const auto replyData = QByteArray("<?xml version=\"1.0\"?>\n"
                                          "<d:prop xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
                                          " <nc:lock>1</nc:lock>\n"
                                          " <nc:lock-owner-type>0</nc:lock-owner-type>\n"
                                          " <nc:lock-owner>alice</nc:lock-owner>\n"
                                          " <nc:lock-owner-displayname>Alice Doe</nc:lock-owner-displayname>\n"
                                          " <nc:lock-owner-editor>Text</nc:lock-owner-editor>\n"
                                          " <nc:lock-time>1650619678</nc:lock-time>\n"
                                          " <nc:lock-timeout>300</nc:lock-timeout>\n"
                                          " <nc:lock-token>files_lock/310997d7-0aae-4e48-97e1-eeb6be6e2202</nc:lock-token>\n"
                                          "</d:prop>\n");

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([replyData] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK") ||
                                                                 request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK"))) {
                reply = new FakeErrorReply(op, request, nullptr, PreconditionFailedHttpErrorCode, replyData);
            }

            return reply;
        });

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + testFileName,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::UnlockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobFailure.wait());
        QCOMPARE(jobSuccess.count(), 0);
    }

    void testSyncLockedFilesAlmostExpired()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QSignalSpy spySyncCompleted(&fakeFolder.syncEngine(), &OCC::SyncEngine::finished);

        ItemCompletedSpy completeSpy(fakeFolder);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::UnlockedItem);
        OCC::SyncJournalFileRecord fileRecordBefore;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordBefore));
        QVERIFY(!fileRecordBefore._lockstate._locked);

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileLocked, 1, QStringLiteral("Nextcloud Office"), {}, QStringLiteral("richdocuments"), QDateTime::currentDateTime().toSecsSinceEpoch() - 1220, 1226);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::LockedItem);
        OCC::SyncJournalFileRecord fileRecordLocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordLocked));
        QVERIFY(fileRecordLocked._lockstate._locked);

        spySyncCompleted.clear();

        QTest::qWait(5000);

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileUnlocked, {}, {}, {}, {}, {}, {});

        QCOMPARE(spySyncCompleted.count(), 0);
        QVERIFY(spySyncCompleted.wait(3000));
        QCOMPARE(spySyncCompleted.count(), 1);

        OCC::SyncJournalFileRecord fileRecordUnlocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordUnlocked));
        QVERIFY(!fileRecordUnlocked._lockstate._locked);
    }

    void testSyncLockedFilesNoExpiredLockedFiles()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QSignalSpy spySyncCompleted(&fakeFolder.syncEngine(), &OCC::SyncEngine::finished);

        ItemCompletedSpy completeSpy(fakeFolder);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::UnlockedItem);
        OCC::SyncJournalFileRecord fileRecordBefore;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordBefore));
        QVERIFY(!fileRecordBefore._lockstate._locked);

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileLocked, 1, QStringLiteral("Nextcloud Office"), {}, QStringLiteral("richdocuments"), QDateTime::currentDateTime().toSecsSinceEpoch() - 1220, 1226);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::LockedItem);
        OCC::SyncJournalFileRecord fileRecordLocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordLocked));
        QVERIFY(fileRecordLocked._lockstate._locked);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        spySyncCompleted.clear();

        QTest::qWait(1000);

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileUnlocked, {}, {}, {}, {}, {}, {});

        QCOMPARE(spySyncCompleted.count(), 0);
        QVERIFY(!spySyncCompleted.wait(3000));
        QCOMPARE(spySyncCompleted.count(), 0);

        OCC::SyncJournalFileRecord fileRecordUnlocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordUnlocked));
        QVERIFY(fileRecordUnlocked._lockstate._locked);
    }

    void testSyncLockedFiles()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        int nGET = 0, nPUT = 0;
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)
            Q_UNUSED(request)

            if (op == QNetworkAccessManager::PutOperation) {
                ++nPUT;
            } else if (op == QNetworkAccessManager::GetOperation) {
                ++nGET;
            }

            return nullptr;
        });

        ItemCompletedSpy completeSpy(fakeFolder);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(nPUT, 0);

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::UnlockedItem);
        OCC::SyncJournalFileRecord fileRecordBefore;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordBefore));
        QVERIFY(!fileRecordBefore._lockstate._locked);

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileLocked, 1, QStringLiteral("Nextcloud Office"), {}, QStringLiteral("richdocuments"), QDateTime::currentDateTime().toSecsSinceEpoch(), 1226);
        fakeFolder.remoteModifier().setModTimeKeepEtag(QStringLiteral("A/a1"), QDateTime::currentDateTime());
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(nGET, 1);
        QCOMPARE(nPUT, 0);

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::LockedItem);
        OCC::SyncJournalFileRecord fileRecordLocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordLocked));
        QVERIFY(fileRecordLocked._lockstate._locked);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(nGET, 1);
        QCOMPARE(nPUT, 0);

        OCC::SyncJournalFileRecord fileRecordAfter;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordAfter));
        QVERIFY(fileRecordAfter._lockstate._locked);

        auto expectedState = fakeFolder.currentLocalState();
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }

    void testLockFile_lockedFileReadOnly_afterSync()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        ItemCompletedSpy completeSpy(fakeFolder);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::UnlockedItem);
        OCC::SyncJournalFileRecord fileRecordBefore;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordBefore));
        QVERIFY(!fileRecordBefore._lockstate._locked);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto localFileNotLocked = QFileInfo{fakeFolder.localPath() + u"A/a1"};
        QVERIFY(localFileNotLocked.isWritable());

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileLocked, 1, QStringLiteral("Nextcloud Office"), {}, QStringLiteral("richdocuments"), QDateTime::currentDateTime().toSecsSinceEpoch(), 1226);
        fakeFolder.remoteModifier().setModTimeKeepEtag(QStringLiteral("A/a1"), QDateTime::currentDateTime());
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_locked, OCC::SyncFileItem::LockStatus::LockedItem);
        OCC::SyncJournalFileRecord fileRecordLocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &fileRecordLocked));
        QVERIFY(fileRecordLocked._lockstate._locked);

        auto expectedState = fakeFolder.currentLocalState();
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);

        const auto localFileLocked = QFileInfo{fakeFolder.localPath() + u"A/a1"};
        QVERIFY(!localFileLocked.isWritable());
    }

    void testLockFile_lockFile_detect_newly_uploaded()
    {
        const auto testFileName = QStringLiteral("document.docx");
        const auto testLockFileName = QStringLiteral(".~lock.document.docx#");

        const auto testDocumentsDirName = "documents";

        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.localModifier().mkdir(testDocumentsDirName);

        fakeFolder.syncEngine().account()->setCapabilities({{"files", QVariantMap{{"locking", QByteArray{"1.0"}}}}});
        QSignalSpy lockFileDetectedNewlyUploadedSpy(&fakeFolder.syncEngine(), &OCC::SyncEngine::lockFileDetected);

        fakeFolder.localModifier().insert(testDocumentsDirName + QStringLiteral("/") + testLockFileName);
        fakeFolder.localModifier().insert(testDocumentsDirName + QStringLiteral("/") + testFileName);

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(lockFileDetectedNewlyUploadedSpy.count(), 1);
    }

    void testLockFile_verifyE2eeFilesUseCorrectPath()
    {
        const auto e2eeRoot = QStringLiteral("encrypted");
        const auto cleartextFilePath = QStringLiteral("encrypted/document.odt");
        const auto encryptedFilePath = QStringLiteral("encrypted/1e4c70c057994f9daf7bbab71b046d5b");

        FakeFolder fakeFolder{FileInfo{}};

        fakeFolder.localModifier().mkdir(e2eeRoot);
        fakeFolder.remoteModifier().mkdir(e2eeRoot);
        fakeFolder.localModifier().insert(cleartextFilePath);

        QVERIFY(fakeFolder.syncOnce());

        // modify local entry for the file to be locked to pretend it's E2E encrypted
        OCC::SyncJournalFileRecord record;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(cleartextFilePath, &record));
        record._e2eEncryptionStatus = OCC::SyncJournalFileRecord::EncryptionStatus::EncryptedMigratedV2_0;
        record._e2eMangledName = encryptedFilePath.toUtf8();
        record._path = cleartextFilePath.toUtf8();
        QVERIFY(fakeFolder.syncJournal().setFileRecord(record));

        // do something similar on the remote -- the encrypted file has a different name
        fakeFolder.remoteModifier().rename(cleartextFilePath, encryptedFilePath);
        fakeFolder.remoteModifier().setE2EE(encryptedFilePath, true);

        // another sync run should not fail now, even with our pretended E2Ee setup :-)
        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(),
                                        &fakeFolder.syncJournal(),
                                        QStringLiteral("/") + cleartextFilePath,
                                        QStringLiteral("/"),
                                        fakeFolder.localPath(),
                                        {},
                                        OCC::SyncFileItem::LockStatus::LockedItem,
                                        OCC::SyncFileItem::LockOwnerType::UserLock);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        QString lockRequestPath;
        connect(fakeFolder.networkAccessManager(), &QNetworkAccessManager::finished, [&lockRequestPath](QNetworkReply *reply) {
            const auto request = reply->request();
            if (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() != QStringLiteral("LOCK")) {
                return;
            }

            QVERIFY(lockRequestPath.isEmpty());
            lockRequestPath = request.url().path();
        });

        job->start();

        QVERIFY(jobSuccess.wait());
        QCOMPARE(jobFailure.count(), 0);

        // expect the path of the LOCK request to have used the mangled name
        QVERIFY(!lockRequestPath.isEmpty());
        QVERIFY(lockRequestPath.contains(encryptedFilePath));
        QVERIFY(!lockRequestPath.contains(cleartextFilePath));

        auto fileRecord = OCC::SyncJournalFileRecord{};
        QVERIFY(fakeFolder.syncJournal().getFileRecord(cleartextFilePath, &fileRecord));
        QVERIFY(fileRecord.isE2eEncrypted());
        QCOMPARE(fileRecord.e2eMangledName(), encryptedFilePath);
        QCOMPARE(fileRecord._lockstate._locked, true);
        QCOMPARE(fileRecord._lockstate._lockEditorApp, QString{});
        QCOMPARE(fileRecord._lockstate._lockOwnerDisplayName, QStringLiteral("John Doe"));
        QCOMPARE(fileRecord._lockstate._lockOwnerId, QStringLiteral("admin"));
        QCOMPARE(fileRecord._lockstate._lockOwnerType, static_cast<qint64>(OCC::SyncFileItem::LockOwnerType::UserLock));
        QCOMPARE(fileRecord._lockstate._lockTime, 1234560);
        QCOMPARE(fileRecord._lockstate._lockTimeout, 1800);

        QVERIFY(fakeFolder.syncOnce());
    }

    void testUploadLockedFilesInDeletedFolder()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        int nGET = 0, nPUT = 0;
        auto verifyLackOfTokenIfHeader = false;
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)
            Q_UNUSED(request)

            if (op == QNetworkAccessManager::PutOperation) {
                ++nPUT;
                if (verifyLackOfTokenIfHeader) {
                    if (request.hasRawHeader("If")) {
                        Q_ASSERT(false);
                    }
                }
            } else if (op == QNetworkAccessManager::GetOperation) {
                ++nGET;
            }

            return nullptr;
        });

        ItemCompletedSpy completeSpy(fakeFolder);

        const auto cleanUpHelper = [&nGET, &nPUT, &completeSpy] () {
            nGET = 0;
            nPUT = 0;
            completeSpy.clear();
        };

        fakeFolder.localModifier().mkdir(u"parent"_s);

        cleanUpHelper();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(nPUT, 0);

        fakeFolder.localModifier().mkdir(u"parent/child"_s);

        cleanUpHelper();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(nPUT, 0);

        fakeFolder.localModifier().insert(u"parent/child/hello.odt"_s);

        cleanUpHelper();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(nPUT, 1);

        OCC::SyncJournalFileRecord record;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(u"parent/child/hello.odt"_s, &record));
        QVERIFY(record.isValid());
        record._lockstate._locked = true;
        record._lockstate._lockToken = u"azertyuiop"_s;
        record._lockstate._lockOwnerType = static_cast<qint64>(OCC::SyncFileItem::LockOwnerType::TokenLock);
        QVERIFY(fakeFolder.syncJournal().setFileRecord(record));
        qDebug() << fakeFolder.localPath() + u"parent/child/.~lock.hello.odt#"_s;
        QFile newLockFile(fakeFolder.localPath() + u"parent/child/.~lock.hello.odt#"_s);
        newLockFile.open(QFile::OpenModeFlag::NewOnly);
        OCC::FileSystem::setFileHidden(fakeFolder.localPath() + u"parent/child/.~lock.hello.odt#"_s, true);

        fakeFolder.remoteModifier().remove(u"parent/child"_s);
        fakeFolder.localModifier().insert(u"parent/child/hello.odt"_s, 128);

        cleanUpHelper();
        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem, {u"parent"_s, u"parent/child"_s, u"parent/child/.~lock.hello.odt#"_s, u"parent/child/hello.odt"_s});
        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(nPUT, 1);
        fakeFolder.syncJournal().wipeErrorBlacklistCategory(OCC::SyncJournalErrorBlacklistRecord::Normal);

        fakeFolder.remoteModifier().mkdir(u"parent/child"_s);
        cleanUpHelper();
        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem, {u"parent"_s, u"parent/child"_s, u"parent/child/.~lock.hello.odt#"_s, u"parent/child/hello.odt"_s});
        verifyLackOfTokenIfHeader = true;
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(nPUT, 1);
    }
};

QTEST_GUILESS_MAIN(TestLockFile)
#include "testlockfile.moc"
