/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "testutils/syncenginetestutils.h"
#include <syncengine.h>
#include "common/ownsql.h"

using namespace OCC;

static void applyPermissionsFromName(FileInfo &info) {
    static QRegularExpression rx(QStringLiteral("_PERM_([^_]*)_[^/]*$"));
    auto m = rx.match(info.name);
    if (m.hasMatch()) {
        info.permissions = RemotePermissions::fromServerString(m.captured(1));
    }

    for (FileInfo &sub : info.children)
        applyPermissionsFromName(sub);
}

// Check if the expected rows in the DB are non-empty. Note that in some cases they might be, then we cannot use this function
// https://github.com/owncloud/client/issues/2038
static void assertCsyncJournalOk(SyncJournalDb &journal)
{
    // The DB is openend in locked mode: close to allow us to access.
    journal.close();

    SqlDatabase db;
    QVERIFY(db.openReadOnly(journal.databaseFilePath()));
    SqlQuery q("SELECT count(*) from metadata where length(fileId) == 0", db);
    QVERIFY(q.exec());
    QVERIFY(q.next().hasData);
    QCOMPARE(q.intValue(0), 0);
#if defined(Q_OS_WIN) // Make sure the file does not appear in the FileInfo
    FileSystem::setFileHidden(journal.databaseFilePath() + QStringLiteral("-shm"), true);
#endif
    journal.allowReopen();
}

SyncFileItemPtr findDiscoveryItem(const SyncFileItemSet &spy, const QString &path)
{
    for (const auto &item : spy) {
        if (item->destination() == path)
            return item;
    }
    return SyncFileItemPtr(new SyncFileItem);
}

bool itemInstruction(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
    Q_ASSERT(!item.isNull());
    return item->instruction() == instr;
}

bool discoveryInstruction(const SyncFileItemSet &spy, const QString &path, const SyncInstructions instr)
{
    auto item = findDiscoveryItem(spy, path);
    return item->instruction() == instr;
}

class TestPermissions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");
        QTest::addColumn<bool>("filesAreDehydrated");

        QTest::newRow("Vfs::Off") << Vfs::Off << false;

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;

            // TODO: the hydrated version will fail due to an issue in the winvfs plugin, so leave it disabled for now.
            // QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi");
        }
    }

    void t7pl()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (vfsMode == Vfs::WindowsCfApi) {
            QSKIP("Known issue: the winvfs plug-in fails to set file permissions");
        }

        FakeFolder fakeFolder(FileInfo(), vfsMode, filesAreDehydrated);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Some of this test depends on the order of discovery. With threading
        // that order becomes effectively random, but we want to make sure to test
        // all cases and thus disable threading.
        auto syncOpts = fakeFolder.syncEngine().syncOptions();
        syncOpts._parallelNetworkJobs = 1;
        fakeFolder.syncEngine().setSyncOptions(syncOpts);

        const auto cannotBeModifiedSize = 133_B;
        const auto canBeModifiedSize = 144_B;

        //create some files
        auto insertIn = [&](const QString &dir) {
            fakeFolder.remoteModifier().insert(dir + QStringLiteral("normalFile_PERM_WVND_.data"), 100_B);
            fakeFolder.remoteModifier().insert(dir + QStringLiteral("cannotBeRemoved_PERM_WVN_.data"), 101_B);
            fakeFolder.remoteModifier().insert(dir + QStringLiteral("canBeRemoved_PERM_D_.data"), 102_B);
            fakeFolder.remoteModifier().insert(dir + QStringLiteral("cannotBeModified_PERM_DVN_.data"), cannotBeModifiedSize, 'A');
            fakeFolder.remoteModifier().insert(dir + QStringLiteral("canBeModified_PERM_W_.data"), canBeModifiedSize);
        };

        //put them in some directories
        fakeFolder.remoteModifier().mkdir(QStringLiteral("normalDirectory_PERM_CKDNV_"));
        insertIn(QStringLiteral("normalDirectory_PERM_CKDNV_/"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("readonlyDirectory_PERM_M_"));
        insertIn(QStringLiteral("readonlyDirectory_PERM_M_/"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_"));
        fakeFolder.remoteModifier().insert(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"), 100_B);
        applyPermissionsFromName(fakeFolder.remoteModifier());

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        qInfo("Do some changes and see how they propagate");

        //1. remove the file than cannot be removed
        //  (they should be recovered)
        fakeFolder.localModifier().remove(QStringLiteral("normalDirectory_PERM_CKDNV_/cannotBeRemoved_PERM_WVN_.data"));
        fakeFolder.localModifier().remove(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data"));

        //2. remove the file that can be removed
        //  (they should properly be gone)
        auto removeReadOnly = [&] (const QString &file)  {
            QVERIFY(!QFileInfo(fakeFolder.localPath() + file).permission(QFile::WriteOwner));
            QFile(fakeFolder.localPath() + file).setPermissions(QFile::WriteOwner | QFile::ReadOwner);
            fakeFolder.localModifier().remove(file);
        };
        removeReadOnly(QStringLiteral("normalDirectory_PERM_CKDNV_/canBeRemoved_PERM_D_.data"));
        removeReadOnly(QStringLiteral("readonlyDirectory_PERM_M_/canBeRemoved_PERM_D_.data"));

        //3. Edit the files that cannot be modified
        //  (they should be recovered, and a conflict shall be created)
        auto editReadOnly = [&] (const QString &file)  {
            QVERIFY(!QFileInfo(fakeFolder.localPath() + file).permission(QFile::WriteOwner));
            QFile(fakeFolder.localPath() + file).setPermissions(QFile::WriteOwner | QFile::ReadOwner);
            fakeFolder.localModifier().appendByte(file);
        };
        editReadOnly(QStringLiteral("normalDirectory_PERM_CKDNV_/cannotBeModified_PERM_DVN_.data"));
        editReadOnly(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"));

        //4. Edit other files
        //  (they should be uploaded)
        fakeFolder.localModifier().appendByte(QStringLiteral("normalDirectory_PERM_CKDNV_/canBeModified_PERM_W_.data"));
        fakeFolder.localModifier().appendByte(QStringLiteral("readonlyDirectory_PERM_M_/canBeModified_PERM_W_.data"));

        //5. Create a new file in a read write folder
        // (should be uploaded)
        fakeFolder.localModifier().insert(QStringLiteral("normalDirectory_PERM_CKDNV_/newFile_PERM_WDNV_.data"), 106_B);
        applyPermissionsFromName(fakeFolder.remoteModifier());

        //do the sync
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        auto currentLocalState = fakeFolder.currentLocalState();

        //1.
        // File should be recovered
        QVERIFY(currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/cannotBeRemoved_PERM_WVN_.data")));
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data")));

        //2.
        // File should be deleted
        QVERIFY(!currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/canBeRemoved_PERM_D_.data")));
        QVERIFY(!currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/canBeRemoved_PERM_D_.data")));

        //3.
        // File should be recovered
        QCOMPARE(currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/cannotBeModified_PERM_DVN_.data"))->contentSize, cannotBeModifiedSize);
        QCOMPARE(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"))->contentSize, cannotBeModifiedSize);
        // and conflict created
        auto c1 = findConflict(currentLocalState, QStringLiteral("normalDirectory_PERM_CKDNV_/cannotBeModified_PERM_DVN_.data"));
        QVERIFY(c1);
        QCOMPARE(c1->contentSize, cannotBeModifiedSize + 1);
        auto c2 = findConflict(currentLocalState, QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"));
        QVERIFY(c2);
        QCOMPARE(c2->contentSize, cannotBeModifiedSize + 1);
        // remove the conflicts for the next state comparison
        fakeFolder.localModifier().remove(c1->path());
        fakeFolder.localModifier().remove(c2->path());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        //4. File should be updated, that's tested by assertLocalAndRemoteDir
        QCOMPARE(currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/canBeModified_PERM_W_.data"))->contentSize, canBeModifiedSize + 1);
        QCOMPARE(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/canBeModified_PERM_W_.data"))->contentSize, canBeModifiedSize + 1);

        //5.
        // the file should be in the server and local
        QVERIFY(currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/newFile_PERM_WDNV_.data")));

        // Both side should still be the same
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Next test

        //6. Create a new file in a read only folder
        // (they should not be uploaded)
        fakeFolder.localModifier().insert(QStringLiteral("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data"), 105_B);

        applyPermissionsFromName(fakeFolder.remoteModifier());
        // error: can't upload to readonly
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();

        //6.
        // The file should not exist on the remote, but still be there
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data")));
        // remove it so next test succeed.
        fakeFolder.localModifier().remove(QStringLiteral("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        // Both side should still be the same
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "remove the read only directory" );
        // -> It must be recovered
        fakeFolder.localModifier().remove(QStringLiteral("readonlyDirectory_PERM_M_"));
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data")));
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_")));
        // the subdirectory had delete permissions, so the contents were deleted
        QVERIFY(!currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // restore
        fakeFolder.remoteModifier().mkdir(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_"));
        fakeFolder.remoteModifier().insert(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "move a directory in a outside read only folder" );

        //Missing directory should be restored
        //new directory should be uploaded
        fakeFolder.localModifier().rename(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_"), QStringLiteral("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_"));
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        currentLocalState = fakeFolder.currentLocalState();

        // old name restored
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_")));
        // contents moved (had move permissions)
        QVERIFY(!currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_")));
        QVERIFY(!currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data")));

        // new still exist  (and is uploaded)
        QVERIFY(currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data")));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // restore for further tests
        fakeFolder.remoteModifier().mkdir(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_"));
        fakeFolder.remoteModifier().insert(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "rename a directory in a read only folder and move a directory to a read-only" );

        // do a sync to update the database
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        QVERIFY(
            fakeFolder.currentLocalState().find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data")));

        //1. rename a directory in a read only folder
        //Missing directory should be restored
        //new directory should stay but not be uploaded
        fakeFolder.localModifier().rename(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_"), QStringLiteral("readonlyDirectory_PERM_M_/newname_PERM_CK_"));

        //2. move a directory from read to read only  (move the directory from previous step)
        fakeFolder.localModifier().rename(QStringLiteral("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_"), QStringLiteral("readonlyDirectory_PERM_M_/moved_PERM_CK_"));

        // error: can't upload to readonly!
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        currentLocalState = fakeFolder.currentLocalState();

        //1.
        // old name restored
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_")));
        // including contents
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data")));
        // new still exist
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/newname_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data")));
        // but is not on server: so remove it localy for the future comarison
        fakeFolder.localModifier().remove(QStringLiteral("readonlyDirectory_PERM_M_/newname_PERM_CK_"));

        //2.
        // old removed
        QVERIFY(!currentLocalState.find(QStringLiteral("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_")));
        // but still on the server: the rename causing an error meant the deletes didn't execute
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_")));
        // new still there
        QVERIFY(currentLocalState.find(QStringLiteral("readonlyDirectory_PERM_M_/moved_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data")));
        //but not on server
        fakeFolder.localModifier().remove(QStringLiteral("readonlyDirectory_PERM_M_/moved_PERM_CK_"));
        fakeFolder.remoteModifier().remove(QStringLiteral("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "multiple restores of a file create different conflict files" );

        fakeFolder.remoteModifier().insert(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"));
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        editReadOnly(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"));
        fakeFolder.localModifier().setContents(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"), FileModifier::DefaultFileSize, 's');
        //do the sync
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        QThread::sleep(1); // make sure changes have different mtime
        editReadOnly(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"));
        fakeFolder.localModifier().setContents(QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"), FileModifier::DefaultFileSize, 'd');

        //do the sync
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        // there should be two conflict files
        currentLocalState = fakeFolder.currentLocalState();
        int count = 0;
        while (auto i = findConflict(currentLocalState, QStringLiteral("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data"))) {
            QVERIFY((i->contentChar == 's') || (i->contentChar == 'd'));
            fakeFolder.localModifier().remove(i->path());
            QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
            currentLocalState = fakeFolder.currentLocalState();
            count++;
        }
        QCOMPARE(count, 2);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    static void setAllPerm(FileInfo *fi, RemotePermissions perm)
    {
        fi->permissions = perm;
        for (auto &subFi : fi->children)
            setAllPerm(&subFi, perm);
    }

    // What happens if the source can't be moved or the target can't be created?
    void testForbiddenMoves()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (vfsMode == Vfs::WindowsCfApi) {
            QSKIP("Known issue: the winvfs plug-in fails to set file permissions");
        }

        FakeFolder fakeFolder(FileInfo(), vfsMode, filesAreDehydrated);

        // Some of this test depends on the order of discovery. With threading
        // that order becomes effectively random, but we want to make sure to test
        // all cases and thus disable threading.
        auto syncOpts = fakeFolder.syncEngine().syncOptions();
        syncOpts._parallelNetworkJobs = 1;
        fakeFolder.syncEngine().setSyncOptions(syncOpts);

        auto &lm = fakeFolder.localModifier();
        auto &rm = fakeFolder.remoteModifier();
        rm.mkdir(QStringLiteral("allowed"));
        rm.mkdir(QStringLiteral("norename"));
        rm.mkdir(QStringLiteral("nomove"));
        rm.mkdir(QStringLiteral("nocreatefile"));
        rm.mkdir(QStringLiteral("nocreatedir"));
        rm.mkdir(QStringLiteral("zallowed")); // order of discovery matters

        rm.mkdir(QStringLiteral("allowed/sub"));
        rm.mkdir(QStringLiteral("allowed/sub2"));
        rm.insert(QStringLiteral("allowed/file"));
        rm.insert(QStringLiteral("allowed/sub/file"));
        rm.insert(QStringLiteral("allowed/sub2/file"));
        rm.mkdir(QStringLiteral("norename/sub"));
        rm.insert(QStringLiteral("norename/file"));
        rm.insert(QStringLiteral("norename/sub/file"));
        rm.mkdir(QStringLiteral("nomove/sub"));
        rm.insert(QStringLiteral("nomove/file"));
        rm.insert(QStringLiteral("nomove/sub/file"));
        rm.mkdir(QStringLiteral("zallowed/sub"));
        rm.mkdir(QStringLiteral("zallowed/sub2"));
        rm.insert(QStringLiteral("zallowed/file"));
        rm.insert(QStringLiteral("zallowed/sub/file"));
        rm.insert(QStringLiteral("zallowed/sub2/file"));

        setAllPerm(rm.find(QStringLiteral("norename")), RemotePermissions::fromServerString(QStringLiteral("WDVCK")));
        setAllPerm(rm.find(QStringLiteral("nomove")), RemotePermissions::fromServerString(QStringLiteral("WDNCK")));
        setAllPerm(rm.find(QStringLiteral("nocreatefile")), RemotePermissions::fromServerString(QStringLiteral("WDNVK")));
        setAllPerm(rm.find(QStringLiteral("nocreatedir")), RemotePermissions::fromServerString(QStringLiteral("WDNVC")));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        // Renaming errors
        lm.rename(QStringLiteral("norename/file"), QStringLiteral("norename/file_renamed"));
        lm.rename(QStringLiteral("norename/sub"), QStringLiteral("norename/sub_renamed"));
        // Moving errors
        lm.rename(QStringLiteral("nomove/file"), QStringLiteral("allowed/file_moved"));
        lm.rename(QStringLiteral("nomove/sub"), QStringLiteral("allowed/sub_moved"));
        // Createfile errors
        lm.rename(QStringLiteral("allowed/file"), QStringLiteral("nocreatefile/file"));
        lm.rename(QStringLiteral("zallowed/file"), QStringLiteral("nocreatefile/zfile"));
        lm.rename(QStringLiteral("allowed/sub"), QStringLiteral("nocreatefile/sub")); // TODO: probably forbidden because it contains file children?
        // Createdir errors
        lm.rename(QStringLiteral("allowed/sub2"), QStringLiteral("nocreatedir/sub2"));
        lm.rename(QStringLiteral("zallowed/sub2"), QStringLiteral("nocreatedir/zsub2"));

        // also hook into discovery!!
        SyncFileItemSet discovery;
        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&discovery](auto v) { discovery = v; });
        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        // if renaming doesn't work, just delete+create
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("norename/file"), CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("norename/sub"), CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(discoveryInstruction(discovery, QStringLiteral("norename/sub"), CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("norename/file_renamed"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("norename/sub_renamed"), CSYNC_INSTRUCTION_NEW));
        // the contents can _move_
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("norename/sub_renamed/file"), CSYNC_INSTRUCTION_RENAME));

        // simiilarly forbidding moves becomes delete+create
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nomove/file"), CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nomove/sub"), CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(discoveryInstruction(discovery, QStringLiteral("nomove/sub"), CSYNC_INSTRUCTION_REMOVE));
        // nomove/sub/file is removed as part of the dir
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("allowed/file_moved"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("allowed/sub_moved"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("allowed/sub_moved/file"), CSYNC_INSTRUCTION_NEW));

        // when moving to an invalid target, the targets should be an error
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatefile/file"), CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatefile/zfile"), CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatefile/sub"), CSYNC_INSTRUCTION_RENAME)); // TODO: What does a real server say?
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatedir/sub2"), CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatedir/zsub2"), CSYNC_INSTRUCTION_ERROR));

        // and the sources of the invalid moves should be restored, not deleted
        // (depending on the order of discovery a follow-up sync is needed)
        QVERIFY(completeSpy.findItem(QStringLiteral("allowed/file")).isNull());
        QVERIFY(completeSpy.findItem(QStringLiteral("allowed/sub2")).isNull());
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("zallowed/file"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("zallowed/sub2"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("zallowed/sub2/file"), CSYNC_INSTRUCTION_NEW));
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), AnotherSyncNeeded::ImmediateFollowUp);

        // A follow-up sync will restore allowed/file and allowed/sub2 and maintain the nocreatedir/file errors
        completeSpy.clear();
        QCOMPARE(fakeFolder.syncJournal().wipeErrorBlacklist(), 4);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatefile/file"), CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatefile/zfile"), CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatedir/sub2"), CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("nocreatedir/zsub2"), CSYNC_INSTRUCTION_ERROR));

        QVERIFY(itemInstruction(completeSpy, QStringLiteral("allowed/file"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("allowed/sub2"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("allowed/sub2/file"), CSYNC_INSTRUCTION_NEW));

        auto cls = fakeFolder.currentLocalState();
        QVERIFY(cls.find(QStringLiteral("allowed/file")));
        QVERIFY(cls.find(QStringLiteral("allowed/sub2")));
        QVERIFY(cls.find(QStringLiteral("zallowed/file")));
        QVERIFY(cls.find(QStringLiteral("zallowed/sub2")));
        QVERIFY(cls.find(QStringLiteral("zallowed/sub2/file")));
    }

    // Test for issue #7293
    void testAllowedMoveForbiddenDelete() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (vfsMode == Vfs::WindowsCfApi) {
            QSKIP("Known issue: the winvfs plug-in fails to set file permissions");
        }

        FakeFolder fakeFolder(FileInfo(), vfsMode, filesAreDehydrated);

        // Some of this test depends on the order of discovery. With threading
        // that order becomes effectively random, but we want to make sure to test
        // all cases and thus disable threading.
        auto syncOpts = fakeFolder.syncEngine().syncOptions();
        syncOpts._parallelNetworkJobs = 1;
        fakeFolder.syncEngine().setSyncOptions(syncOpts);

        auto &lm = fakeFolder.localModifier();
        auto &rm = fakeFolder.remoteModifier();
        rm.mkdir(QStringLiteral("changeonly"));
        rm.mkdir(QStringLiteral("changeonly/sub1"));
        rm.insert(QStringLiteral("changeonly/sub1/file1"));
        rm.insert(QStringLiteral("changeonly/sub1/filetorname1a"));
        rm.insert(QStringLiteral("changeonly/sub1/filetorname1z"));
        rm.mkdir(QStringLiteral("changeonly/sub2"));
        rm.insert(QStringLiteral("changeonly/sub2/file2"));
        rm.insert(QStringLiteral("changeonly/sub2/filetorname2a"));
        rm.insert(QStringLiteral("changeonly/sub2/filetorname2z"));

        setAllPerm(rm.find(QStringLiteral("changeonly")), RemotePermissions::fromServerString(QStringLiteral("NSV")));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        lm.rename(QStringLiteral("changeonly/sub1/filetorname1a"), QStringLiteral("changeonly/sub1/aaa1_renamed"));
        lm.rename(QStringLiteral("changeonly/sub1/filetorname1z"), QStringLiteral("changeonly/sub1/zzz1_renamed"));

        lm.rename(QStringLiteral("changeonly/sub2/filetorname2a"), QStringLiteral("changeonly/sub2/aaa2_renamed"));
        lm.rename(QStringLiteral("changeonly/sub2/filetorname2z"), QStringLiteral("changeonly/sub2/zzz2_renamed"));

        lm.rename(QStringLiteral("changeonly/sub1"), QStringLiteral("changeonly/aaa"));
        lm.rename(QStringLiteral("changeonly/sub2"), QStringLiteral("changeonly/zzz"));


        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestPermissions)
#include "testpermissions.moc"
