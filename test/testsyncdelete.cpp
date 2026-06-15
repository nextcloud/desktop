/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

class TestSyncDelete : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testDeleteDirectoryWithNewFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        // Remove a directory on the server with new files on the client
        fakeFolder.remoteModifier().remove("A");
        fakeFolder.localModifier().insert("A/hello.txt");

        // Symmetry
        fakeFolder.localModifier().remove("B");
        fakeFolder.remoteModifier().insert("B/hello.txt");

        QVERIFY(fakeFolder.syncOnce());

        // A/a1 must be gone because the directory was removed on the server, but hello.txt must be there
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/hello.txt"));

        // Symmetry
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/hello.txt"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Files blocked from upload because of quota errors must survive remote parent folder deletion.
    void testQuotaBlockedFileProtectedFromParentFolderDeletion()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        // Establish an initial clean sync: a1 and a2 exist on both sides.
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a new local file that will fail to upload due to remote quota (HTTP 507).
        fakeFolder.localModifier().insert(QStringLiteral("A/quota_blocked.txt"), 100);
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/quota_blocked.txt"), 507);

        // Upload fails so file is blacklisted as InsufficientRemoteStorage.
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/quota_blocked.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // The file was never uploaded.
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/quota_blocked.txt")));

        // Remove the server error so further requests to that path don't return 507.
        fakeFolder.serverErrorPaths().clear();

        // Server side: folder A is moved/deleted.
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));

        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.syncOnce();

        // File must survive local deletion when its parent folder is removed on the server.
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/quota_blocked.txt")));

        // Parent folder A must also survive because it still contains the protected file.
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A")));

        // Previously synced siblings must have been deleted locally (trust the server).
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));

        // File must remain absent on the server.
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/quota_blocked.txt")));

        // Must be reported as an error with the quota message, not silently ignored.
        {
            auto item = completeSpy.findItem(QStringLiteral("A/quota_blocked.txt"));
            QVERIFY(item);
            QCOMPARE(item->_instruction, CSYNC_INSTRUCTION_ERROR);
            QVERIFY(item->_status == SyncFileItem::SoftError || item->_status == SyncFileItem::NormalError);
            QVERIFY(!item->_errorString.contains(QStringLiteral("skipped due to earlier error")));
        }
    }

    // Files blocked from upload because of quota errors must survive parent folder rename then delete.
    void testQuotaBlockedFileProtectedAfterParentFolderMoveThenDelete()
    {
        // Use a custom initial state with only folder A to avoid rename collisions.
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("a1"), 4},
                {QStringLiteral("a2"), 4},
            }},
        }};
        FakeFolder fakeFolder{initialState};

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // New local file that fails to upload due to quota.
        fakeFolder.localModifier().insert(QStringLiteral("A/quota_blocked.txt"), 100);
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/quota_blocked.txt"), 507);

        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/quota_blocked.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/quota_blocked.txt")));

        // Switch the quota error to the new path so the file remains blocked after the rename.
        fakeFolder.serverErrorPaths().clear();
        fakeFolder.serverErrorPaths().append(QStringLiteral("D/quota_blocked.txt"), 507);

        // Server renames A to D. The blacklist entry stays at "A/quota_blocked.txt" until
        // the next sync updates it; the local file follows to "D/".
        fakeFolder.remoteModifier().rename(QStringLiteral("A"), QStringLiteral("D"));
        // Sync: local A is renamed to D; any upload attempt for D/quota_blocked.txt fails with 507.
        fakeFolder.syncOnce();

        // Blacklist entry must have been updated to the new path.
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("D/quota_blocked.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("D/quota_blocked.txt")));

        // Server deletes folder D. Keep quota error active to verify the file retries.
        fakeFolder.serverErrorPaths().append(QStringLiteral("D/quota_blocked.txt"), 507);
        fakeFolder.remoteModifier().remove(QStringLiteral("D"));
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.syncOnce();

        // File must not have been deleted locally.
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("D/quota_blocked.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("D")));

        // Previously synced siblings must be gone.
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("D/a1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("D/a2")));

        // Must be reported as a failed upload, not silently ignored.
        {
            const auto item = completeSpy.findItem(QStringLiteral("D/quota_blocked.txt"));
            QVERIFY(item);
            QCOMPARE(item->_instruction, CSYNC_INSTRUCTION_ERROR);
            QVERIFY(item->_status == SyncFileItem::SoftError || item->_status == SyncFileItem::NormalError);
            QVERIFY(!item->_errorString.contains(QStringLiteral("skipped due to earlier error")));
        }
    }

    // Regression: new files in a server deleted folder must still be deleted locally
    void testDeleteDirectoryWithNewFileNoQuotaError()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.localModifier().insert(QStringLiteral("A/newfile.txt"), 100);

        QVERIFY(fakeFolder.syncOnce());

        // New local file with no quota blacklist entry must be removed when parent is deleted on server.
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/newfile.txt")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // A file blocked from upload due to a quota error is protected from local deletion even
    // when the folder is deleted on the server in the same sync where the file was first added
    // locally. No prior blacklist entry exists; the parent folder's last known DB quota is used.
    void testQuotaBlockedFileProtectedInSameSyncAsParentFolderDeletion()
    {
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("small.txt"), 4},
            }},
        }};
        // Skip the automatic initial sync so the quota is set before the first sync,
        // ensuring the DB stores bytesAvailable=50 for folder A.
        FakeFolder fakeFolder{initialState, {}, {}, false};

        fakeFolder.remoteModifier().setFolderQuota(QStringLiteral("A"), FileInfo::FolderQuota{0, 50});
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Server deletes A and a large local file is added before the next sync,
        // so no blacklist entry exists yet for the file.
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.localModifier().insert(QStringLiteral("A/big_file.txt"), 100); // 100 > 50

        ItemCompletedSpy completeSpy(fakeFolder);
        QSignalSpy storageSpy(&fakeFolder.syncEngine(), &SyncEngine::syncError);
        fakeFolder.syncOnce();

        // big_file.txt was never uploaded but exceeds the last known quota.
        // It must be protected locally rather than silently deleted.
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/big_file.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A")));

        // Previously synced sibling must be removed (trust the server deletion).
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/small.txt")));

        // Must be reported as a quota error item.
        {
            const auto item = completeSpy.findItem(QStringLiteral("A/big_file.txt"));
            QVERIFY(item);
            QCOMPARE(item->_instruction, CSYNC_INSTRUCTION_ERROR);
            QVERIFY(item->_status == SyncFileItem::SoftError || item->_status == SyncFileItem::NormalError);
        }

        // The storage full notification must fire even though no upload was attempted
        // and there is no blacklist entry yet for the file.
        const bool hadStorageNotification = std::any_of(storageSpy.begin(), storageSpy.end(),
            [](const QList<QVariant> &args) {
                return args.at(1).value<ErrorCategory>() == ErrorCategory::InsufficientRemoteStorage;
            });
        QVERIFY(hadStorageNotification);
    }

    // Files blocked from upload because of quota errors must follow their parent folder through
    // multiple successive server side renames.
    void testQuotaBlockedFileFollowsParentFolderMove()
    {
        // Initial state: folder A with small.txt and empty sibling B.
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("small.txt"), 4},
            }},
            {QStringLiteral("B"), {}},
        }};
        FakeFolder fakeFolder{initialState};

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Block big.txt from uploading permanently (507).
        fakeFolder.localModifier().insert(QStringLiteral("A/big.txt"), 1000);
        fakeFolder.setServerOverride([](QNetworkAccessManager::Operation op,
                                        const QNetworkRequest &req,
                                        QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation
                && req.url().path().contains(QLatin1String("big.txt"))) {
                return new FakeErrorReply{op, req, nullptr, 507};
            }
            return nullptr;
        });

        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/big.txt")));

        // Server: move A into B (server now has B/A). Client must follow the rename.
        fakeFolder.remoteModifier().rename(QStringLiteral("A"), QStringLiteral("B/A"));
        fakeFolder.syncOnce();

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A/big.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A/small.txt")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("B/A/big.txt")));

        // Server: rename B/A to B/C.
        fakeFolder.remoteModifier().rename(QStringLiteral("B/A"), QStringLiteral("B/C"));
        fakeFolder.syncOnce();

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/C")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/A")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/C/big.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/C/small.txt")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("B/C/big.txt")));

        // Server: move B/C to root (C). Client must follow without creating a ghost B/C.
        fakeFolder.remoteModifier().rename(QStringLiteral("B/C"), QStringLiteral("C"));
        fakeFolder.syncOnce();

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("C/big.txt")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/C")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/C/big.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("C/small.txt")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("C/big.txt")));
    }

    // After a server side folder rename, if the renamed folder is then deleted on the server,
    // files inside it that were blocked from upload because of quota errors must still be
    // protected from local deletion. The blacklist path is updated during the rename sync, so
    // the subsequent deletion sync must find the entry at the new path.
    void testQuotaBlockedFileProtectedAfterFolderRenameAndDelete()
    {
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("small.txt"), 4},
            }},
            {QStringLiteral("B"), {}},
        }};
        FakeFolder fakeFolder{initialState};
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Block big.txt from uploading permanently (507) regardless of its path.
        // This simulates a server where storage is genuinely full.
        fakeFolder.localModifier().insert(QStringLiteral("A/big.txt"), 1000);
        fakeFolder.setServerOverride([](QNetworkAccessManager::Operation op,
                                        const QNetworkRequest &req,
                                        QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation
                && req.url().path().contains(QLatin1String("big.txt"))) {
                return new FakeErrorReply{op, req, nullptr, 507};
            }
            return nullptr;
        });
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Server moves A into B. Client must follow and keep big.txt at the new path.
        fakeFolder.remoteModifier().rename(QStringLiteral("A"), QStringLiteral("B/A"));
        fakeFolder.syncOnce();
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A/big.txt")));
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("B/A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Server deletes B/A. big.txt must be protected from local deletion.
        fakeFolder.remoteModifier().remove(QStringLiteral("B/A"));
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A/big.txt")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/A/small.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B")));
    }

    // Same as above but rename and delete happen between client syncs so the client never sees B/A on server.
    void testQuotaBlockedFileProtectedAfterFolderRenameAndDeleteInOnePoll()
    {
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("small.txt"), 4},
            }},
            {QStringLiteral("B"), {}},
        }};
        FakeFolder fakeFolder{initialState};
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(QStringLiteral("A/big.txt"), 1000);
        fakeFolder.setServerOverride([](QNetworkAccessManager::Operation op,
                                        const QNetworkRequest &req,
                                        QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation
                && req.url().path().contains(QLatin1String("big.txt"))) {
                return new FakeErrorReply{op, req, nullptr, 507};
            }
            return nullptr;
        });
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Server renames A to B/A and then immediately deletes B/A before the client polls again.
        fakeFolder.remoteModifier().rename(QStringLiteral("A"), QStringLiteral("B/A"));
        fakeFolder.remoteModifier().remove(QStringLiteral("B/A"));

        // big.txt must survive: it was never uploaded and A is gone from server entirely.
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/big.txt")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/small.txt")));
    }

    // After the client restarts following a protection event, a subsequent server-side deletion of
    // the same folder must still protect quota-blocked files. The protection relies on the blacklist
    // entry surviving the restart sync (where the folder is re-created via MKDIR) and then being
    // found again when the folder is deleted a second time.
    void testQuotaBlockedFileProtectedAfterClientRestart()
    {
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("small.txt"), 4},
            }},
        }};
        FakeFolder fakeFolder{initialState};
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(QStringLiteral("A/big.txt"), 1000);
        fakeFolder.setServerOverride([](QNetworkAccessManager::Operation op,
                                        const QNetworkRequest &req,
                                        QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation
                && req.url().path().contains(QLatin1String("big.txt"))) {
                return new FakeErrorReply{op, req, nullptr, 507};
            }
            return nullptr;
        });
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Server deletes A. Protection kicks in: big.txt survives, A's DB record is cleared.
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/big.txt")));

        // Simulate client restart: next sync re-creates A on the server via MKDIR,
        // then big.txt upload fails again because storage is still full.
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A")));
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Simulate a quiet sync after the client restart: nothing changed locally so the real
        // client uses DatabaseAndFilesystem with an empty path set. A uses ParentNotChanged
        // which skips local discovery, so big.txt (not in DB) never appears in _syncItems.
        // deleteStaleErrorBlacklistEntries must not prune its InsufficientRemoteStorage entry.
        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        fakeFolder.syncOnce();
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Server deletes A again. The real client uses DatabaseAndFilesystem with no local paths
        // (server-triggered sync, no local changes). The fix in startAsync() must force NormalQuery
        // for A's subdirectory job so that big.txt is discovered and checkNewDeleteConflict fires.
        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/big.txt")));
    }

    // Files blocked from upload because of quota errors must retry on the next sync once quota is freed.
    void testQuotaBlockedFileRetriesWhenQuotaResolved()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a new local file that will fail to upload due to quota (HTTP 507).
        fakeFolder.localModifier().insert(QStringLiteral("A/quota_blocked.txt"), 100);
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/quota_blocked.txt"), 507);

        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/quota_blocked.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
            // Must be 0 so the file retries on the very next sync rather than backing off.
            QCOMPARE(entry._ignoreDuration, 0);
        }
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/quota_blocked.txt")));

        // Quota freed, remove the server error.
        fakeFolder.serverErrorPaths().clear();

        // Next sync: file must retry immediately (ignoreDuration = 0) and upload.
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/quota_blocked.txt")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Blacklist entry must be cleared after a successful upload.
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/quota_blocked.txt"));
            QVERIFY(!entry.isValid());
        }
    }

    // The storage full notification must fire in the same sync as a folder rename, not only when the upload is retried.
    void testStorageNotificationEmittedOnParentFolderRename()
    {
        FileInfo initialState{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("small.txt"), 4},
            }},
        }};
        FakeFolder fakeFolder{initialState};

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Block big.txt from uploading permanently (quota exceeded).
        fakeFolder.localModifier().insert(QStringLiteral("A/big.txt"), 1000);
        fakeFolder.setServerOverride([](QNetworkAccessManager::Operation op,
                                        const QNetworkRequest &req,
                                        QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation
                && req.url().path().contains(QLatin1String("big.txt"))) {
                return new FakeErrorReply{op, req, nullptr, 507};
            }
            return nullptr;
        });

        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("A/big.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }

        // Server renames A to B. The storage full notification must fire in this sync,
        // not only after the next retry of the upload.
        fakeFolder.remoteModifier().rename(QStringLiteral("A"), QStringLiteral("B"));
        QSignalSpy storageSpy(&fakeFolder.syncEngine(), &SyncEngine::syncError);
        fakeFolder.syncOnce();

        const bool hadStorageNotification = std::any_of(storageSpy.begin(), storageSpy.end(),
            [](const QList<QVariant> &args) {
                return args.at(1).value<ErrorCategory>() == ErrorCategory::InsufficientRemoteStorage;
            });
        QVERIFY(hadStorageNotification);
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/big.txt")));
    }

    // After a file blocked from upload because of quota errors is protected from deletion inside a
    // server deleted folder, the client must keep the folder locally even after the user removes
    // the file. The folder's DB record is cleared during the protection sync so the next sync
    // treats it as a new local folder rather than a server deleted one.
    void testQuotaProtectedFolderKeptAfterLocalFileDeletion()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Quota block: upload fails with 507.
        fakeFolder.localModifier().insert(QStringLiteral("A/quota_blocked.txt"), 100);
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/quota_blocked.txt"), 507);
        QVERIFY(!fakeFolder.syncOnce());
        fakeFolder.serverErrorPaths().clear();

        // Server deletes folder A — file is protected locally.
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.syncOnce();
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/quota_blocked.txt")));

        // User manually removes the file blocked from upload because of quota errors.
        fakeFolder.localModifier().remove(QStringLiteral("A/quota_blocked.txt"));

        // Next sync: folder A is empty but must remain locally — the client must not delete it.
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A")));
    }

    // Same as testQuotaProtectedFolderKeptAfterLocalFileDeletion but with a nested folder (B/A)
    // where the parent B remains on the server. Without invalidating B's cached etag after the
    // protection sync, the next sync would hit ParentNotChanged for B and treat B/A as still
    // present via its DB record, preventing the folder from being treated as new local.
    void testQuotaProtectedNestedFolderKeptAfterLocalFileDeletion()
    {
        // Start with a structure that has a nested subfolder B/A.
        FileInfo initialState{QString{}, {
            {QStringLiteral("B"), {
                {QStringLiteral("A"), {
                    {QStringLiteral("file1.txt"), 4},
                    {QStringLiteral("file2.txt"), 4},
                }},
            }},
        }};
        FakeFolder fakeFolder{initialState};

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a file inside B/A that fails to upload due to quota (HTTP 507).
        fakeFolder.localModifier().insert(QStringLiteral("B/A/quota_blocked.txt"), 100);
        fakeFolder.serverErrorPaths().append(QStringLiteral("B/A/quota_blocked.txt"), 507);
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(QStringLiteral("B/A/quota_blocked.txt"));
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
        }
        fakeFolder.serverErrorPaths().clear();

        // Server deletes subfolder B/A (parent B remains). The file blocked from upload because
        // of quota errors must be protected locally even though B still exists and may have a
        // cached etag.
        fakeFolder.remoteModifier().remove(QStringLiteral("B/A"));
        fakeFolder.syncOnce();
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A/quota_blocked.txt")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A")));

        // User manually deletes the file blocked from upload because of quota errors.
        fakeFolder.localModifier().remove(QStringLiteral("B/A/quota_blocked.txt"));

        // Next sync: B/A is empty but must remain locally.
        // B's etag was invalidated during the protection sync so the client re-queries B from
        // the server instead of relying on the stale DB cache.
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/A")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B")));
    }

    void issue1329()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        fakeFolder.localModifier().remove("B");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a directory that was just removed in the previous sync:
        fakeFolder.localModifier().mkdir("B");
        fakeFolder.localModifier().insert("B/b1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("B/b1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
