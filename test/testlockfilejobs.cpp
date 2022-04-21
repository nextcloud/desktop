#include "lockfilejobs.h"

#include "account.h"
#include "accountstate.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "syncenginetestutils.h"

#include <QTest>
#include <QSignalSpy>

class TestLockFileJobs : public QObject
{
    Q_OBJECT

public:
    TestLockFileJobs() = default;

private slots:
    void initTestCase()
    {
    }

    void testLockFileJob_lockFile_jobSuccess()
    {
        const auto testFileName = QStringLiteral("file.txt");
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto job = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::LockedItem);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobSuccess.wait());
        QCOMPARE(jobFailure.count(), 0);

        auto fileRecord = OCC::SyncJournalFileRecord{};
        QVERIFY(fakeFolder.syncJournal().getFileRecord(testFileName, &fileRecord));
        QCOMPARE(fileRecord._locked, true);
        QCOMPARE(fileRecord._lockEditorApp, QString{});
        QCOMPARE(fileRecord._lockOwnerDisplayName, QStringLiteral("John Doe"));
        QCOMPARE(fileRecord._lockOwnerId, QStringLiteral("john"));
        QCOMPARE(fileRecord._lockOwnerType, static_cast<qint64>(OCC::SyncFileItem::LockOwnerType::UserLock));
        QCOMPARE(fileRecord._lockTime, 1234560);
        QCOMPARE(fileRecord._lockTimeout, 1800);

        QVERIFY(fakeFolder.syncOnce());
    }

    void testLockFileJob_lockFile_unlockFile_jobSuccess()
    {
        const auto testFileName = QStringLiteral("file.txt");
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(testFileName);

        QVERIFY(fakeFolder.syncOnce());

        auto lockFileJob = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::LockedItem);

        QSignalSpy lockFileJobSuccess(lockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy lockFileJobFailure(lockFileJob, &OCC::LockFileJob::finishedWithError);

        lockFileJob->start();

        QVERIFY(lockFileJobSuccess.wait());
        QCOMPARE(lockFileJobFailure.count(), 0);

        QVERIFY(fakeFolder.syncOnce());

        auto unlockFileJob = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::UnlockedItem);

        QSignalSpy unlockFileJobSuccess(unlockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy unlockFileJobFailure(unlockFileJob, &OCC::LockFileJob::finishedWithError);

        unlockFileJob->start();

        QVERIFY(unlockFileJobSuccess.wait());
        QCOMPARE(unlockFileJobFailure.count(), 0);

        auto fileRecord = OCC::SyncJournalFileRecord{};
        QVERIFY(fakeFolder.syncJournal().getFileRecord(testFileName, &fileRecord));
        QCOMPARE(fileRecord._locked, false);

        QVERIFY(fakeFolder.syncOnce());
    }

    void testLockFileJob_lockFile_alreadyLocked()
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

        auto job = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::LockedItem);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobFailure.wait());
        QCOMPARE(jobSuccess.count(), 0);
    }

    void testLockFileJob_unlockFile_alreadyUnlocked()
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

        auto job = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::LockedItem);

        QSignalSpy jobSuccess(job, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy jobFailure(job, &OCC::LockFileJob::finishedWithError);

        job->start();

        QVERIFY(jobFailure.wait());
        QCOMPARE(jobSuccess.count(), 0);
    }

    void testLockFileJob_lockFile_jobError()
    {
        const auto testFileName = QStringLiteral("file.txt");
        static constexpr auto InternalServerErrorHttpErrorCode = 500;

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.setServerOverride([] (FakeQNAM::Operation op, const QNetworkRequest &request, QIODevice *) {
            QNetworkReply *reply = nullptr;
            if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("LOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, InternalServerErrorHttpErrorCode, {});
            } else if (op == QNetworkAccessManager::CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("UNLOCK")) {
                reply = new FakeErrorReply(op, request, nullptr, InternalServerErrorHttpErrorCode, {});
            }

            return reply;
        });

        fakeFolder.localModifier().insert(QStringLiteral("file.txt"));

        QVERIFY(fakeFolder.syncOnce());

        auto lockFileJob = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::LockedItem);

        QSignalSpy lockFileJobSuccess(lockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy lockFileJobFailure(lockFileJob, &OCC::LockFileJob::finishedWithError);

        lockFileJob->start();

        QVERIFY(lockFileJobFailure.wait());
        QCOMPARE(lockFileJobSuccess.count(), 0);

        QVERIFY(fakeFolder.syncOnce());

        auto unlockFileJob = new OCC::LockFileJob(fakeFolder.account(), &fakeFolder.syncJournal(), QStringLiteral("/") + testFileName, OCC::SyncFileItem::LockStatus::UnlockedItem);

        QSignalSpy unlockFileJobSuccess(unlockFileJob, &OCC::LockFileJob::finishedWithoutError);
        QSignalSpy unlockFileJobFailure(unlockFileJob, &OCC::LockFileJob::finishedWithError);

        unlockFileJob->start();

        QVERIFY(unlockFileJobFailure.wait());
        QCOMPARE(unlockFileJobSuccess.count(), 0);

        QVERIFY(fakeFolder.syncOnce());
    }
};

QTEST_MAIN(TestLockFileJobs)
#include "testlockfilejobs.moc"
