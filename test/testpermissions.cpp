/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>
#include "common/ownsql.h"

using namespace OCC;

static void applyPermissionsFromName(FileInfo &info) {
    static QRegularExpression rx("_PERM_([^_]*)_[^/]*$");
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
    FileSystem::setFileHidden(journal.databaseFilePath() + "-shm", true);
#endif
}

SyncFileItemPtr findDiscoveryItem(const SyncFileItemVector &spy, const QString &path)
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
    return item->_instruction == instr;
}

bool discoveryInstruction(const SyncFileItemVector &spy, const QString &path, const SyncInstructions instr)
{
    auto item = findDiscoveryItem(spy, path);
    return item->_instruction == instr;
}

class TestPermissions : public QObject
{
    Q_OBJECT

private slots:

    void t7pl()
    {
        FakeFolder fakeFolder{ FileInfo() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Some of this test depends on the order of discovery. With threading
        // that order becomes effectively random, but we want to make sure to test
        // all cases and thus disable threading.
        auto syncOpts = fakeFolder.syncEngine().syncOptions();
        syncOpts._parallelNetworkJobs = 1;
        fakeFolder.syncEngine().setSyncOptions(syncOpts);

        const int cannotBeModifiedSize = 133;
        const int canBeModifiedSize = 144;

        //create some files
        auto insertIn = [&](const QString &dir) {
            fakeFolder.remoteModifier().insert(dir + "normalFile_PERM_WVND_.data", 100 );
            fakeFolder.remoteModifier().insert(dir + "cannotBeRemoved_PERM_WVN_.data", 101 );
            fakeFolder.remoteModifier().insert(dir + "canBeRemoved_PERM_D_.data", 102 );
            fakeFolder.remoteModifier().insert(dir + "cannotBeModified_PERM_DVN_.data", cannotBeModifiedSize , 'A');
            fakeFolder.remoteModifier().insert(dir + "canBeModified_PERM_W_.data", canBeModifiedSize );
        };

        //put them in some directories
        fakeFolder.remoteModifier().mkdir("normalDirectory_PERM_CKDNV_");
        insertIn("normalDirectory_PERM_CKDNV_/");
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_M_" );
        insertIn("readonlyDirectory_PERM_M_/" );
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_M_/subdir_PERM_CK_");
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_");
        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data", 100);
        applyPermissionsFromName(fakeFolder.remoteModifier());

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        qInfo("Do some changes and see how they propagate");

        //1. remove the file than cannot be removed
        //  (they should be recovered)
        fakeFolder.localModifier().remove("normalDirectory_PERM_CKDNV_/cannotBeRemoved_PERM_WVN_.data");
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data");

        //2. remove the file that can be removed
        //  (they should properly be gone)
        auto removeReadOnly = [&] (const QString &file)  {
            QVERIFY(!QFileInfo(fakeFolder.localPath() + file).permission(QFile::WriteOwner));
            QFile(fakeFolder.localPath() + file).setPermissions(QFile::WriteOwner | QFile::ReadOwner);
            fakeFolder.localModifier().remove(file);
        };
        removeReadOnly("normalDirectory_PERM_CKDNV_/canBeRemoved_PERM_D_.data");
        removeReadOnly("readonlyDirectory_PERM_M_/canBeRemoved_PERM_D_.data");

        //3. Edit the files that cannot be modified
        //  (they should be recovered, and a conflict shall be created)
        auto editReadOnly = [&] (const QString &file)  {
            QVERIFY(!QFileInfo(fakeFolder.localPath() + file).permission(QFile::WriteOwner));
            QFile(fakeFolder.localPath() + file).setPermissions(QFile::WriteOwner | QFile::ReadOwner);
            fakeFolder.localModifier().appendByte(file);
        };
        editReadOnly("normalDirectory_PERM_CKDNV_/cannotBeModified_PERM_DVN_.data");
        editReadOnly("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data");

        //4. Edit other files
        //  (they should be uploaded)
        fakeFolder.localModifier().appendByte("normalDirectory_PERM_CKDNV_/canBeModified_PERM_W_.data");
        fakeFolder.localModifier().appendByte("readonlyDirectory_PERM_M_/canBeModified_PERM_W_.data");

        //5. Create a new file in a read write folder
        // (should be uploaded)
        fakeFolder.localModifier().insert("normalDirectory_PERM_CKDNV_/newFile_PERM_WDNV_.data", 106 );
        applyPermissionsFromName(fakeFolder.remoteModifier());

        //do the sync
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        auto currentLocalState = fakeFolder.currentLocalState();

        //1.
        // File should be recovered
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNV_/cannotBeRemoved_PERM_WVN_.data"));
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data"));

        //2.
        // File should be deleted
        QVERIFY(!currentLocalState.find("normalDirectory_PERM_CKDNV_/canBeRemoved_PERM_D_.data"));
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_M_/canBeRemoved_PERM_D_.data"));

        //3.
        // File should be recovered
        QCOMPARE(currentLocalState.find("normalDirectory_PERM_CKDNV_/cannotBeModified_PERM_DVN_.data")->size, cannotBeModifiedSize);
        QCOMPARE(currentLocalState.find("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data")->size, cannotBeModifiedSize);
        // and conflict created
        auto c1 = findConflict(currentLocalState, "normalDirectory_PERM_CKDNV_/cannotBeModified_PERM_DVN_.data");
        QVERIFY(c1);
        QCOMPARE(c1->size, cannotBeModifiedSize + 1);
        auto c2 = findConflict(currentLocalState, "readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data");
        QVERIFY(c2);
        QCOMPARE(c2->size, cannotBeModifiedSize + 1);
        // remove the conflicts for the next state comparison
        fakeFolder.localModifier().remove(c1->path());
        fakeFolder.localModifier().remove(c2->path());

        //4. File should be updated, that's tested by assertLocalAndRemoteDir
        QCOMPARE(currentLocalState.find("normalDirectory_PERM_CKDNV_/canBeModified_PERM_W_.data")->size, canBeModifiedSize + 1);
        QCOMPARE(currentLocalState.find("readonlyDirectory_PERM_M_/canBeModified_PERM_W_.data")->size, canBeModifiedSize + 1);

        //5.
        // the file should be in the server and local
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNV_/newFile_PERM_WDNV_.data"));

        // Both side should still be the same
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Next test

        //6. Create a new file in a read only folder
        // (they should not be uploaded)
        fakeFolder.localModifier().insert("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data", 105 );

        applyPermissionsFromName(fakeFolder.remoteModifier());
        // error: can't upload to readonly
        QVERIFY(!fakeFolder.syncOnce());

        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();

        //6.
        // The file should not exist on the remote, but still be there
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data"));
        // remove it so next test succeed.
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data");
        // Both side should still be the same
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "remove the read only directory" );
        // -> It must be recovered
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data"));
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_"));
        // the subdirectory had delete permissions, so the contents were deleted
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // restore
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_");
        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "move a directory in a outside read only folder" );

        //Missing directory should be restored
        //new directory should be uploaded
        fakeFolder.localModifier().rename("readonlyDirectory_PERM_M_/subdir_PERM_CK_", "normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        currentLocalState = fakeFolder.currentLocalState();

        // old name restored
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_"));
        // contents moved (had move permissions)
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_"));
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));

        // new still exist  (and is uploaded)
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // restore for further tests
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_");
        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "rename a directory in a read only folder and move a directory to a read-only" );

        // do a sync to update the database
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        QVERIFY(fakeFolder.currentLocalState().find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));

        //1. rename a directory in a read only folder
        //Missing directory should be restored
        //new directory should stay but not be uploaded
        fakeFolder.localModifier().rename("readonlyDirectory_PERM_M_/subdir_PERM_CK_", "readonlyDirectory_PERM_M_/newname_PERM_CK_"  );

        //2. move a directory from read to read only  (move the directory from previous step)
        fakeFolder.localModifier().rename("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_", "readonlyDirectory_PERM_M_/moved_PERM_CK_" );

        // error: can't upload to readonly!
        QVERIFY(!fakeFolder.syncOnce());
        currentLocalState = fakeFolder.currentLocalState();

        //1.
        // old name restored
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_" ));
        // including contents
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));
        // new still exist
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/newname_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));
        // but is not on server: so remove it localy for the future comarison
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_/newname_PERM_CK_");

        //2.
        // old removed
        QVERIFY(!currentLocalState.find("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_"));
        // but still on the server: the rename causing an error meant the deletes didn't execute
        QVERIFY(fakeFolder.currentRemoteState().find("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_"));
        // new still there
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/moved_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));
        //but not on server
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_/moved_PERM_CK_");
        fakeFolder.remoteModifier().remove("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_");

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "multiple restores of a file create different conflict files" );

        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());

        editReadOnly("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data");
        fakeFolder.localModifier().setContents("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data", 's');
        //do the sync
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        QThread::sleep(1); // make sure changes have different mtime
        editReadOnly("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data");
        fakeFolder.localModifier().setContents("readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data", 'd');

        //do the sync
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        // there should be two conflict files
        currentLocalState = fakeFolder.currentLocalState();
        int count = 0;
        while (auto i = findConflict(currentLocalState, "readonlyDirectory_PERM_M_/cannotBeModified_PERM_DVN_.data")) {
            QVERIFY((i->contentChar == 's') || (i->contentChar == 'd'));
            fakeFolder.localModifier().remove(i->path());
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
        FakeFolder fakeFolder{FileInfo{}};

        // Some of this test depends on the order of discovery. With threading
        // that order becomes effectively random, but we want to make sure to test
        // all cases and thus disable threading.
        auto syncOpts = fakeFolder.syncEngine().syncOptions();
        syncOpts._parallelNetworkJobs = 1;
        fakeFolder.syncEngine().setSyncOptions(syncOpts);

        auto &lm = fakeFolder.localModifier();
        auto &rm = fakeFolder.remoteModifier();
        rm.mkdir("allowed");
        rm.mkdir("norename");
        rm.mkdir("nomove");
        rm.mkdir("nocreatefile");
        rm.mkdir("nocreatedir");
        rm.mkdir("zallowed"); // order of discovery matters

        rm.mkdir("allowed/sub");
        rm.mkdir("allowed/sub2");
        rm.insert("allowed/file");
        rm.insert("allowed/sub/file");
        rm.insert("allowed/sub2/file");
        rm.mkdir("norename/sub");
        rm.insert("norename/file");
        rm.insert("norename/sub/file");
        rm.mkdir("nomove/sub");
        rm.insert("nomove/file");
        rm.insert("nomove/sub/file");
        rm.mkdir("zallowed/sub");
        rm.mkdir("zallowed/sub2");
        rm.insert("zallowed/file");
        rm.insert("zallowed/sub/file");
        rm.insert("zallowed/sub2/file");

        setAllPerm(rm.find("norename"), RemotePermissions::fromServerString("WDVCK"));
        setAllPerm(rm.find("nomove"), RemotePermissions::fromServerString("WDNCK"));
        setAllPerm(rm.find("nocreatefile"), RemotePermissions::fromServerString("WDNVK"));
        setAllPerm(rm.find("nocreatedir"), RemotePermissions::fromServerString("WDNVC"));

        QVERIFY(fakeFolder.syncOnce());

        // Renaming errors
        lm.rename("norename/file", "norename/file_renamed");
        lm.rename("norename/sub", "norename/sub_renamed");
        // Moving errors
        lm.rename("nomove/file", "allowed/file_moved");
        lm.rename("nomove/sub", "allowed/sub_moved");
        // Createfile errors
        lm.rename("allowed/file", "nocreatefile/file");
        lm.rename("zallowed/file", "nocreatefile/zfile");
        lm.rename("allowed/sub", "nocreatefile/sub"); // TODO: probably forbidden because it contains file children?
        // Createdir errors
        lm.rename("allowed/sub2", "nocreatedir/sub2");
        lm.rename("zallowed/sub2", "nocreatedir/zsub2");

        // also hook into discovery!!
        SyncFileItemVector discovery;
        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&discovery](auto v) { discovery = v; });
        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(!fakeFolder.syncOnce());

        // if renaming doesn't work, just delete+create
        QVERIFY(itemInstruction(completeSpy, "norename/file", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "norename/sub", CSYNC_INSTRUCTION_NONE));
        QVERIFY(discoveryInstruction(discovery, "norename/sub", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "norename/file_renamed", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "norename/sub_renamed", CSYNC_INSTRUCTION_NEW));
        // the contents can _move_
        QVERIFY(itemInstruction(completeSpy, "norename/sub_renamed/file", CSYNC_INSTRUCTION_RENAME));

        // simiilarly forbidding moves becomes delete+create
        QVERIFY(itemInstruction(completeSpy, "nomove/file", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "nomove/sub", CSYNC_INSTRUCTION_NONE));
        QVERIFY(discoveryInstruction(discovery, "nomove/sub", CSYNC_INSTRUCTION_REMOVE));
        // nomove/sub/file is removed as part of the dir
        QVERIFY(itemInstruction(completeSpy, "allowed/file_moved", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub_moved", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub_moved/file", CSYNC_INSTRUCTION_NEW));

        // when moving to an invalid target, the targets should be an error
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/file", CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/zfile", CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/sub", CSYNC_INSTRUCTION_RENAME)); // TODO: What does a real server say?
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/sub2", CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/zsub2", CSYNC_INSTRUCTION_ERROR));

        // and the sources of the invalid moves should be restored, not deleted
        // (depending on the order of discovery a follow-up sync is needed)
        QVERIFY(itemInstruction(completeSpy, "allowed/file", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub2", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "zallowed/file", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "zallowed/sub2", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "zallowed/sub2/file", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), ImmediateFollowUp);

        // A follow-up sync will restore allowed/file and allowed/sub2 and maintain the nocreatedir/file errors
        completeSpy.clear();
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(itemInstruction(completeSpy, "nocreatefile/file", CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/zfile", CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/sub2", CSYNC_INSTRUCTION_ERROR));
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/zsub2", CSYNC_INSTRUCTION_ERROR));

        QVERIFY(itemInstruction(completeSpy, "allowed/file", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub2", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub2/file", CSYNC_INSTRUCTION_NEW));

        auto cls = fakeFolder.currentLocalState();
        QVERIFY(cls.find("allowed/file"));
        QVERIFY(cls.find("allowed/sub2"));
        QVERIFY(cls.find("zallowed/file"));
        QVERIFY(cls.find("zallowed/sub2"));
        QVERIFY(cls.find("zallowed/sub2/file"));
    }

    // Test for issue #7293
    void testAllowedMoveForbiddenDelete() {
         FakeFolder fakeFolder{FileInfo{}};

        // Some of this test depends on the order of discovery. With threading
        // that order becomes effectively random, but we want to make sure to test
        // all cases and thus disable threading.
        auto syncOpts = fakeFolder.syncEngine().syncOptions();
        syncOpts._parallelNetworkJobs = 1;
        fakeFolder.syncEngine().setSyncOptions(syncOpts);

        auto &lm = fakeFolder.localModifier();
        auto &rm = fakeFolder.remoteModifier();
        rm.mkdir("changeonly");
        rm.mkdir("changeonly/sub1");
        rm.insert("changeonly/sub1/file1");
        rm.insert("changeonly/sub1/filetorname1a");
        rm.insert("changeonly/sub1/filetorname1z");
        rm.mkdir("changeonly/sub2");
        rm.insert("changeonly/sub2/file2");
        rm.insert("changeonly/sub2/filetorname2a");
        rm.insert("changeonly/sub2/filetorname2z");

        setAllPerm(rm.find("changeonly"), RemotePermissions::fromServerString("NSV"));

        QVERIFY(fakeFolder.syncOnce());

        lm.rename("changeonly/sub1/filetorname1a", "changeonly/sub1/aaa1_renamed");
        lm.rename("changeonly/sub1/filetorname1z", "changeonly/sub1/zzz1_renamed");

        lm.rename("changeonly/sub2/filetorname2a", "changeonly/sub2/aaa2_renamed");
        lm.rename("changeonly/sub2/filetorname2z", "changeonly/sub2/zzz2_renamed");

        lm.rename("changeonly/sub1", "changeonly/aaa");
        lm.rename("changeonly/sub2", "changeonly/zzz");


        auto expectedState = fakeFolder.currentLocalState();

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }
};

QTEST_GUILESS_MAIN(TestPermissions)
#include "testpermissions.moc"
