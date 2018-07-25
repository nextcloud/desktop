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
        info.permissions = RemotePermissions(m.captured(1));
    }

    for (FileInfo &sub : info.children)
        applyPermissionsFromName(sub);
}

const FileInfo *findConflict(FileInfo &dir, const QString &filename)
{
    QFileInfo info(filename);
    const FileInfo *parentDir = dir.find(info.path());
    if (!parentDir) return nullptr;
    QString start = info.baseName() + " (conflicted copy";
    for (const auto &item : parentDir->children) {
        if (item.name.startsWith(start)) {
            return &item;
        }
    }
    return nullptr;
}

// Check if the expected rows in the DB are non-empty. Note that in some cases they might be, then we cannot use this function
// https://github.com/owncloud/client/issues/2038
static void assertCsyncJournalOk(SyncJournalDb &journal)
{
    SqlDatabase db;
    QVERIFY(db.openReadOnly(journal.databaseFilePath()));
    SqlQuery q("SELECT count(*) from metadata where length(fileId) == 0", db);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.intValue(0), 0);
#if defined(Q_OS_WIN) // Make sure the file does not appear in the FileInfo
    FileSystem::setFileHidden(journal.databaseFilePath() + "-shm", true);
#endif
}


class TestPermissions : public QObject
{
    Q_OBJECT

private slots:

    void t7pl()
    {
        FakeFolder fakeFolder{ FileInfo() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

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
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "move a directory in a outside read only folder" );

        //Missing directory should be restored
        //new directory should be uploaded
        fakeFolder.localModifier().rename("readonlyDirectory_PERM_M_/subdir_PERM_CK_", "normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        fakeFolder.syncOnce();
        if (fakeFolder.syncEngine().isAnotherSyncNeeded() ==  ImmediateFollowUp) {
            QVERIFY(fakeFolder.syncOnce());
        }
        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();

        // old name restored
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_"));
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));

        // new still exist  (and is uploaded)
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        //######################################################################
        qInfo( "rename a directory in a read only folder and move a directory to a read-only" );

        // do a sync to update the database
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());

        //1. rename a directory in a read only folder
        //Missing directory should be restored
        //new directory should stay but not be uploaded
        fakeFolder.localModifier().rename("readonlyDirectory_PERM_M_/subdir_PERM_CK_", "readonlyDirectory_PERM_M_/newname_PERM_CK_"  );

        //2. move a directory from read to read only  (move the directory from previous step)
        fakeFolder.localModifier().rename("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_", "readonlyDirectory_PERM_M_/moved_PERM_CK_" );

        // error: can't upload to readonly!
        QVERIFY(!fakeFolder.syncOnce());
        if (fakeFolder.syncEngine().isAnotherSyncNeeded() ==  ImmediateFollowUp) {
            QVERIFY(!fakeFolder.syncOnce());
        }
        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();

        //1.
        // old name restored
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));
        // new still exist
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/newname_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));
        // but is not on server: so remove it localy for the future comarison
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_/newname_PERM_CK_");

        //2.
        // old removed
        QVERIFY(!currentLocalState.find("normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_"));
        // new still there
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_M_/moved_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data" ));
        //but not on server
        fakeFolder.localModifier().remove("readonlyDirectory_PERM_M_/moved_PERM_CK_");

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "multiple restores of a file create different conflict files" );

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

};

QTEST_GUILESS_MAIN(TestPermissions)
#include "testpermissions.moc"
