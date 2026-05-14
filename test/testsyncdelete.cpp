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

        // Server side: folder A is renamed to D. The quota blocked file was never on the
        // server, so the blacklist entry path is still "A/quota_blocked.txt" while the
        // local file moves to "D/" as the client follows the rename.
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

        // Server side: now delete folder D. Keep the quota error active so we verify
        // that the file retries (and fails with a quota error) rather than being silently
        // skipped by the blacklist backoff timer.
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

        // The file must be reported as a failed upload attempt, not silently ignored.
        // With _ignoreDuration = 0 for quota errors, the file retries every sync and
        // produces a real quota error instead of a "skipped due to earlier error" message.
        {
            const auto item = completeSpy.findItem(QStringLiteral("D/quota_blocked.txt"));
            QVERIFY(item);
            QCOMPARE(item->_instruction, CSYNC_INSTRUCTION_NEW);
            QVERIFY(item->_status == SyncFileItem::DetailError || item->_status == SyncFileItem::NormalError);
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
