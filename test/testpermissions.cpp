/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "syncenginetestutils.h"
#include <syncengine.h>
#include "common/ownsql.h"

#include <QtTest>

#include <filesystem>
#include <iostream>

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
    // The DB is opened in locked mode: close to allow us to access.
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

static bool isReadOnlyFolder(const std::wstring &path)
{
    return FileSystem::isFolderReadOnly(std::filesystem::path{path});
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
    const auto checkHelper = [item, instr]() {
        QCOMPARE(item->_instruction, instr);
    };

    checkHelper();
    return item->_instruction == instr;
}

bool discoveryInstruction(const SyncFileItemVector &spy, const QString &path, const SyncInstructions instr)
{
    auto item = findDiscoveryItem(spy, path);
    const auto checkHelper = [item, instr]() {
        QCOMPARE(item->_instruction, instr);
    };

    checkHelper();
    return item->_instruction == instr;
}

class TestPermissions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Logger::instance()->setLogFlush(true);
        Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void t7pl_data()
    {
        QTest::addColumn<bool>("moveToTrashEnabled");
        QTest::newRow("move to trash") << true;
        QTest::newRow("delete") << false;
    }

    void t7pl()
    {
        QFETCH(bool, moveToTrashEnabled);

        FakeFolder fakeFolder{ FileInfo() };

        auto syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._moveFilesToTrash = moveToTrashEnabled;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

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
            fakeFolder.remoteModifier().insert(dir + "normalFile_PERM_GWVND_.data", 100 );
            fakeFolder.remoteModifier().insert(dir + "cannotBeRemoved_PERM_GWVN_.data", 101 );
            fakeFolder.remoteModifier().insert(dir + "canBeRemoved_PERM_GD_.data", 102 );
            fakeFolder.remoteModifier().insert(dir + "cannotBeModified_PERM_GDVN_.data", cannotBeModifiedSize , 'A');
            fakeFolder.remoteModifier().insert(dir + "canBeModified_PERM_GW_.data", canBeModifiedSize );
        };

        //put them in some directories
        fakeFolder.remoteModifier().mkdir("normalDirectory_PERM_CKDNVG_");
        insertIn("normalDirectory_PERM_CKDNVG_/");
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_MG_" );
        insertIn("readonlyDirectory_PERM_MG_/" );
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_");
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_");
        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data", 100);
        applyPermissionsFromName(fakeFolder.remoteModifier());

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        qInfo("Do some changes and see how they propagate");

        const auto removeReadOnly = [&] (const QString &file)  {
            const auto fileInfoToDelete = QFileInfo(fakeFolder.localPath() + file);
            FileSystem::FilePermissionsRestore enabler{fileInfoToDelete.absolutePath(), FileSystem::FolderPermissions::ReadWrite};
            if (!fileInfoToDelete.isDir()) {
                QString errorString;
                const auto result = FileSystem::remove(fileInfoToDelete.absoluteFilePath(), &errorString);
                if (!result) {
                    qDebug() << "fail to delete:" << fileInfoToDelete.absoluteFilePath() << errorString;
                    QVERIFY(result);
                }
            } else {
                const auto result = FileSystem::removeRecursively(fileInfoToDelete.absoluteFilePath());
                if (!result) {
                    qDebug() << "fail to delete:" << fileInfoToDelete.absoluteFilePath();
                    QVERIFY(result);
                }
            }
        };

        const auto renameReadOnly = [&] (const QString &relativePath, const QString &relativeDestinationDirectory)  {
            try {
                const auto sourceFileInfo = QFileInfo(fakeFolder.localPath() + relativePath);
                FileSystem::FilePermissionsRestore sourceEnabler{sourceFileInfo.absolutePath(), FileSystem::FolderPermissions::ReadWrite};

                const auto destinationFileInfo = QFileInfo(fakeFolder.localPath() + relativeDestinationDirectory);
                FileSystem::FilePermissionsRestore destinationEnabler{destinationFileInfo.absolutePath(), FileSystem::FolderPermissions::ReadWrite};

                const auto isSourceReadOnly = !static_cast<bool>(std::filesystem::status(sourceFileInfo.absolutePath().toStdWString()).permissions() & std::filesystem::perms::owner_write);
                const auto isDestinationReadOnly = !static_cast<bool>(std::filesystem::status(destinationFileInfo.absolutePath().toStdWString()).permissions() & std::filesystem::perms::owner_write);
                if (isSourceReadOnly) {
                    std::filesystem::permissions(sourceFileInfo.absolutePath().toStdWString(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
                }
                if (isDestinationReadOnly) {
                    std::filesystem::permissions(destinationFileInfo.absolutePath().toStdWString(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
                }
                fakeFolder.localModifier().rename(relativePath, relativeDestinationDirectory);
                if (isSourceReadOnly) {
                    std::filesystem::permissions(sourceFileInfo.absolutePath().toStdWString(), std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
                }
                if (isDestinationReadOnly) {
                    std::filesystem::permissions(destinationFileInfo.absolutePath().toStdWString(), std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
                }
            }
            catch (const std::exception& e)
            {
                qWarning() << e.what();
            }
        };

        const auto insertReadOnly = [&] (const QString &file, const int fileSize) {
            try {
                const auto fileInfo = QFileInfo(fakeFolder.localPath() + file);
                FileSystem::FilePermissionsRestore enabler{fileInfo.absolutePath(), FileSystem::FolderPermissions::ReadWrite};
                fakeFolder.localModifier().insert(file, fileSize);
            }
            catch (const std::exception& e)
            {
                qWarning() << e.what();
            }
        };

        //1. remove the file than cannot be removed
        //  (they should be recovered)
        fakeFolder.localModifier().remove("normalDirectory_PERM_CKDNVG_/cannotBeRemoved_PERM_GWVN_.data");
        removeReadOnly("readonlyDirectory_PERM_MG_/cannotBeRemoved_PERM_GWVN_.data");

        //2. remove the file that can be removed
        //  (they should properly be gone)
        removeReadOnly("normalDirectory_PERM_CKDNVG_/canBeRemoved_PERM_GD_.data");
        removeReadOnly("readonlyDirectory_PERM_MG_/canBeRemoved_PERM_GD_.data");

        //3. Edit the files that cannot be modified
        //  (they should be recovered, and a conflict shall be created)
        auto editReadOnly = [&] (const QString &file)  {
            QVERIFY(!QFileInfo(fakeFolder.localPath() + file).permission(QFile::WriteOwner));
            FileSystem::setFileReadOnly(fakeFolder.localPath() + file, false);
            fakeFolder.localModifier().appendByte(file);
        };
        editReadOnly("normalDirectory_PERM_CKDNVG_/cannotBeModified_PERM_GDVN_.data");
        editReadOnly("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data");

        //4. Edit other files
        //  (they should be uploaded)
        fakeFolder.localModifier().appendByte("normalDirectory_PERM_CKDNVG_/canBeModified_PERM_GW_.data");
        fakeFolder.localModifier().appendByte("readonlyDirectory_PERM_MG_/canBeModified_PERM_GW_.data");

        //5. Create a new file in a read write folder
        // (should be uploaded)
        fakeFolder.localModifier().insert("normalDirectory_PERM_CKDNVG_/newFile_PERM_GWDNV_.data", 106 );
        applyPermissionsFromName(fakeFolder.remoteModifier());

        //do the sync
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        auto currentLocalState = fakeFolder.currentLocalState();

        //1.
        // File should be recovered
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNVG_/cannotBeRemoved_PERM_GWVN_.data"));
        QCOMPARE(currentLocalState.find("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data")->size, cannotBeModifiedSize);
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/cannotBeRemoved_PERM_GWVN_.data"));

        //2.
        // File should be deleted
        QVERIFY(!currentLocalState.find("normalDirectory_PERM_CKDNVG_/canBeRemoved_PERM_GD_.data"));
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/canBeRemoved_PERM_GD_.data"));

        //3.
        // File should be recovered
        QCOMPARE(currentLocalState.find("normalDirectory_PERM_CKDNVG_/cannotBeModified_PERM_GDVN_.data")->size, cannotBeModifiedSize);
        // and conflict created
        auto c1 = findConflict(currentLocalState, "normalDirectory_PERM_CKDNVG_/cannotBeModified_PERM_GDVN_.data");
        QVERIFY(c1);
        QCOMPARE(c1->size, cannotBeModifiedSize + 1);
        auto c2 = findConflict(currentLocalState, "readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data");
        QVERIFY(c2);
        QCOMPARE(c2->size, cannotBeModifiedSize + 1);
        // remove the conflicts for the next state comparison
        fakeFolder.localModifier().remove(c1->path());
        removeReadOnly(c2->path());

        //4. File should be updated, that's tested by assertLocalAndRemoteDir
        QCOMPARE(currentLocalState.find("normalDirectory_PERM_CKDNVG_/canBeModified_PERM_GW_.data")->size, canBeModifiedSize + 1);
        QCOMPARE(currentLocalState.find("readonlyDirectory_PERM_MG_/canBeModified_PERM_GW_.data")->size, canBeModifiedSize + 1);

        //5.
        // the file should be in the server and local
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNVG_/newFile_PERM_GWDNV_.data"));

        // Both side should still be the same
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Next test

        //6. Create a new file in a read only folder
        // (they should not be uploaded)
        insertReadOnly("readonlyDirectory_PERM_MG_/newFile_PERM_GWDNV_.data", 105 );

        applyPermissionsFromName(fakeFolder.remoteModifier());
        // error: can't upload to readonly
        QVERIFY(fakeFolder.syncOnce());

        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();

        //6.
        // The file should not exist on the remote, and not be there
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/newFile_PERM_GWDNV_.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("readonlyDirectory_PERM_MG_/newFile_PERM_GWDNV_.data"));
        // Both side should still be the same
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "remove the read only directory" );
        // -> It must be recovered
        removeReadOnly("readonlyDirectory_PERM_MG_");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());
        currentLocalState = fakeFolder.currentLocalState();
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/cannotBeRemoved_PERM_GWVN_.data"));
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_"));
        // the subdirectory had delete permissions, but, it was within the recovered directory, so must also get recovered
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "move a directory in a outside read only folder" );

        //Missing directory should be restored
        //new directory should be uploaded
        renameReadOnly("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_", "normalDirectory_PERM_CKDNVG_/subdir_PERM_CKDNVG_");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        currentLocalState = fakeFolder.currentLocalState();

        // old name restored
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_"));
        // contents moved (had move permissions)
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_"));
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data"));

        // new still exist  (and is uploaded)
        QVERIFY(currentLocalState.find("normalDirectory_PERM_CKDNVG_/subdir_PERM_CKDNVG_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // restore for further tests
        fakeFolder.remoteModifier().mkdir("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_");
        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "rename a directory in a read only folder and move a directory to a read-only" );

        // do a sync to update the database
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        QVERIFY(fakeFolder.currentLocalState().find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data" ));

        //1. rename a directory in a read only folder
        //Missing directory should be restored
        //new directory should stay but not be uploaded
        renameReadOnly("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_", "readonlyDirectory_PERM_MG_/newname_PERM_CK_"  );

        //2. move a directory from read to read only  (move the directory from previous step)
        renameReadOnly("normalDirectory_PERM_CKDNVG_/subdir_PERM_CKDNVG_", "readonlyDirectory_PERM_MG_/moved_PERM_CK_" );

        // can't upload to readonly but not an error
        QVERIFY(fakeFolder.syncOnce());
        currentLocalState = fakeFolder.currentLocalState();

        //1.
        // old name restored
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_" ));
        // including contents
        QVERIFY(currentLocalState.find("readonlyDirectory_PERM_MG_/subdir_PERM_CKG_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data" ));
        // new no longer exists
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/newname_PERM_CK_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data" ));
        // but is not on server: should have been locally removed
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/newname_PERM_CK_"));

        //2.
        // old removed
        QVERIFY(!currentLocalState.find("normalDirectory_PERM_CKDNVG_/subdir_PERM_CKDNVG_"));
        // but still on the server: the rename causing an error meant the deletes didn't execute
        QVERIFY(fakeFolder.currentRemoteState().find("normalDirectory_PERM_CKDNVG_/subdir_PERM_CKDNVG_"));
        // new no longer exists
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/moved_PERM_CK_/subsubdir_PERM_CKDNVG_/normalFile_PERM_GWVND_.data" ));
        // should have been cleaned up as invalid item inside read-only folder
        QVERIFY(!currentLocalState.find("readonlyDirectory_PERM_MG_/moved_PERM_CK_"));
        fakeFolder.remoteModifier().remove("normalDirectory_PERM_CKDNVG_/subdir_PERM_CKDNVG_");

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        //######################################################################
        qInfo( "multiple restores of a file create different conflict files" );

        fakeFolder.remoteModifier().insert("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data");
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());

        editReadOnly("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data");
        fakeFolder.localModifier().setContents("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data", 's');
        //do the sync
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        editReadOnly("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data");
        fakeFolder.localModifier().setContents("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data", 'd');
        fakeFolder.localModifier().setModTime("readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data", QDateTime::currentDateTime().addDays(1)); // make sure changes have different mtime

        //do the sync
        applyPermissionsFromName(fakeFolder.remoteModifier());
        QVERIFY(fakeFolder.syncOnce());
        assertCsyncJournalOk(fakeFolder.syncJournal());

        // there should be two conflict files
        currentLocalState = fakeFolder.currentLocalState();
        int count = 0;
        while (auto i = findConflict(currentLocalState, "readonlyDirectory_PERM_MG_/cannotBeModified_PERM_GDVN_.data")) {
            QVERIFY((i->contentChar == 's') || (i->contentChar == 'd'));
            removeReadOnly(i->path());
            currentLocalState = fakeFolder.currentLocalState();
            count++;
        }
        QCOMPARE(count, 2);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    static void setAllPerm(FileInfo *fi, OCC::RemotePermissions perm)
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

        setAllPerm(rm.find("norename"), RemotePermissions::fromServerString("GWDVCK"));
        setAllPerm(rm.find("nomove"), RemotePermissions::fromServerString("GWDNCK"));
        setAllPerm(rm.find("nocreatefile"), RemotePermissions::fromServerString("GWDNVK"));
        setAllPerm(rm.find("nocreatedir"), RemotePermissions::fromServerString("GWDNVC"));

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
        QVERIFY(fakeFolder.syncOnce());

        // if renaming doesn't work, just delete+create
        QVERIFY(itemInstruction(completeSpy, "norename/file", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "norename/sub", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(discoveryInstruction(discovery, "norename/sub", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "norename/file_renamed", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "norename/sub_renamed", CSYNC_INSTRUCTION_NEW));
        // the contents can _move_
        QVERIFY(itemInstruction(completeSpy, "norename/sub_renamed/file", CSYNC_INSTRUCTION_RENAME));

        // simiilarly forbidding moves becomes delete+create
        QVERIFY(itemInstruction(completeSpy, "nomove/file", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "nomove/sub", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(discoveryInstruction(discovery, "nomove/sub", CSYNC_INSTRUCTION_REMOVE));
        // nomove/sub/file is removed as part of the dir
        QVERIFY(itemInstruction(completeSpy, "allowed/file_moved", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub_moved", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "allowed/sub_moved/file", CSYNC_INSTRUCTION_NEW));

        // when moving to an invalid target, the targets should be ignored
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/file", CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/zfile", CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/sub", CSYNC_INSTRUCTION_RENAME)); // TODO: What does a real server say?
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/sub2", CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/zsub2", CSYNC_INSTRUCTION_IGNORE));

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
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(itemInstruction(completeSpy, "nocreatefile/file", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "nocreatefile/zfile", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/sub2", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "nocreatedir/zsub2", CSYNC_INSTRUCTION_NONE));

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

    void testParentMoveNotAllowedChildrenRestored()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &lm = fakeFolder.localModifier();
        auto &rm = fakeFolder.remoteModifier();
        rm.mkdir("forbidden-move");
        rm.mkdir("forbidden-move/sub1");
        rm.insert("forbidden-move/sub1/file1.txt", 100);
        rm.mkdir("forbidden-move/sub2");
        rm.insert("forbidden-move/sub2/file2.txt", 100);

        rm.find("forbidden-move")->permissions = RemotePermissions::fromServerString("WNCKG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        lm.rename("forbidden-move", "forbidden-move-new");

        QVERIFY(fakeFolder.syncOnce());

        // verify that original folder did not get wiped (files are still there)
        QVERIFY(fakeFolder.currentRemoteState().find("forbidden-move/sub1/file1.txt"));
        QVERIFY(fakeFolder.currentRemoteState().find("forbidden-move/sub2/file2.txt"));

        // verify that the duplicate folder has been created when trying to rename a folder that has its move permissions forbidden
        QVERIFY(fakeFolder.currentRemoteState().find("forbidden-move-new/sub1/file1.txt"));
        QVERIFY(fakeFolder.currentRemoteState().find("forbidden-move-new/sub2/file2.txt"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    static void demo_perms(std::filesystem::perms p)
    {
        using std::filesystem::perms;
        auto show = [=](char op, perms perm)
        {
            std::cout << (perms::none == (perm & p) ? '-' : op);
        };
        show('r', perms::owner_read);
        show('w', perms::owner_write);
        show('x', perms::owner_exec);
        show('r', perms::group_read);
        show('w', perms::group_write);
        show('x', perms::group_exec);
        show('r', perms::others_read);
        show('w', perms::others_write);
        show('x', perms::others_exec);
        std::cout << std::endl;
    }

    void testReadOnlyFolderIsReallyReadOnly()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("readOnlyFolder");

        remote.find("readOnlyFolder")->permissions = RemotePermissions::fromServerString("MG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto folderStatus = std::filesystem::status(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("readOnlyFolder")).toStdWString());
        QVERIFY(folderStatus.permissions() & std::filesystem::perms::owner_read);
    }

    void testReadWriteFolderIsReallyReadWrite()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("readWriteFolder");

        remote.find("readWriteFolder")->permissions = RemotePermissions::fromServerString("GCKWDNVRSM");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto folderStatus = std::filesystem::status(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("readWriteFolder")).toStdWString());
        QVERIFY(folderStatus.permissions() & std::filesystem::perms::owner_read);
        QVERIFY(folderStatus.permissions() & std::filesystem::perms::owner_write);
    }

    void testChangePermissionsFolder()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("testFolder");

        remote.find("testFolder")->permissions = RemotePermissions::fromServerString("MG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder")).toStdWString()));

        remote.find("testFolder")->permissions = RemotePermissions::fromServerString("CKWDNVRSMG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(!isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder")).toStdWString()));

        remote.find("testFolder")->permissions = RemotePermissions::fromServerString("MG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder")).toStdWString()));
    }

    void testChangePermissionsForFolderHierarchy()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("testFolder");

        remote.find("testFolder")->permissions = RemotePermissions::fromServerString("MG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        remote.mkdir("testFolder/subFolderReadWrite");
        remote.mkdir("testFolder/subFolderReadOnly");

        remote.find("testFolder/subFolderReadOnly")->permissions = RemotePermissions::fromServerString("mG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder")).toStdWString()));
        QVERIFY(!isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder/subFolderReadWrite")).toStdWString()));
        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder/subFolderReadOnly")).toStdWString()));

        remote.find("testFolder/subFolderReadOnly")->permissions = RemotePermissions::fromServerString("CKWDNVRSmG");
        remote.find("testFolder/subFolderReadWrite")->permissions = RemotePermissions::fromServerString("mG");
        remote.mkdir("testFolder/newSubFolder");
        remote.create("testFolder/testFile", 12, '9');
        remote.create("testFolder/testReadOnlyFile", 13, '8');
        remote.find("testFolder/testReadOnlyFile")->permissions = RemotePermissions::fromServerString("mG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder")).toStdWString()));
        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder/subFolderReadWrite")).toStdWString()));
        QVERIFY(!isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder/subFolderReadOnly")).toStdWString()));

        remote.rename("testFolder/subFolderReadOnly", "testFolder/subFolderReadWriteNew");
        remote.rename("testFolder/subFolderReadWrite", "testFolder/subFolderReadOnlyNew");
        remote.rename("testFolder/testFile", "testFolder/testFileNew");
        remote.rename("testFolder/testReadOnlyFile", "testFolder/testReadOnlyFileNew");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(isReadOnlyFolder(static_cast<QString>(fakeFolder.localPath() + QStringLiteral("testFolder")).toStdWString()));
    }

    void testDeleteChildItemsInReadOnlyFolder()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("readOnlyFolder");
        remote.mkdir("readOnlyFolder/test");
        remote.insert("readOnlyFolder/readOnlyFile.txt");

        remote.find("readOnlyFolder")->permissions = RemotePermissions::fromServerString("MG");
        remote.find("readOnlyFolder/test")->permissions = RemotePermissions::fromServerString("mG");
        remote.find("readOnlyFolder/readOnlyFile.txt")->permissions = RemotePermissions::fromServerString("mG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        remote.remove("readOnlyFolder/test");
        remote.remove("readOnlyFolder/readOnlyFile.txt");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto ensureReadOnlyItem = [&fakeFolder] (const auto path) -> bool {
            const auto itemStatus = std::filesystem::status(static_cast<QString>(fakeFolder.localPath() + path).toStdWString());
            return static_cast<bool>(itemStatus.permissions() & std::filesystem::perms::owner_read);
        };
        QVERIFY(ensureReadOnlyItem("readOnlyFolder"));
    }

    void testRenameChildItemsInReadOnlyFolder()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("readOnlyFolder");
        remote.mkdir("readOnlyFolder/test");
        remote.insert("readOnlyFolder/readOnlyFile.txt");

        remote.find("readOnlyFolder")->permissions = RemotePermissions::fromServerString("MG");
        remote.find("readOnlyFolder/test")->permissions = RemotePermissions::fromServerString("mG");
        remote.find("readOnlyFolder/readOnlyFile.txt")->permissions = RemotePermissions::fromServerString("mG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        remote.rename("readOnlyFolder/test", "readOnlyFolder/testRename");
        remote.rename("readOnlyFolder/readOnlyFile.txt", "readOnlyFolder/readOnlyFileRename.txt");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto ensureReadOnlyItem = [&fakeFolder] (const auto path) -> bool {
            const auto itemStatus = std::filesystem::status(static_cast<QString>(fakeFolder.localPath() + path).toStdWString());
            return static_cast<bool>(itemStatus.permissions() & std::filesystem::perms::owner_read);
        };
        QVERIFY(ensureReadOnlyItem("readOnlyFolder"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/testRename"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/readOnlyFileRename.txt"));
    }

    void testMoveChildItemsInReadOnlyFolder()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("readOnlyFolder");
        remote.mkdir("readOnlyFolder/child");
        remote.mkdir("readOnlyFolder/test");
        remote.insert("readOnlyFolder/readOnlyFile.txt");

        remote.find("readOnlyFolder")->permissions = RemotePermissions::fromServerString("MG");
        remote.find("readOnlyFolder/child")->permissions = RemotePermissions::fromServerString("mG");
        remote.find("readOnlyFolder/test")->permissions = RemotePermissions::fromServerString("mG");
        remote.find("readOnlyFolder/readOnlyFile.txt")->permissions = RemotePermissions::fromServerString("mG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        remote.rename("readOnlyFolder/test", "readOnlyFolder/child/test");
        remote.rename("readOnlyFolder/readOnlyFile.txt", "readOnlyFolder/child/readOnlyFile.txt");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto ensureReadOnlyItem = [&fakeFolder] (const auto path) -> bool {
            const auto itemStatus = std::filesystem::status(static_cast<QString>(fakeFolder.localPath() + path).toStdWString());
            return static_cast<bool>(itemStatus.permissions() & std::filesystem::perms::owner_read);
        };
        QVERIFY(ensureReadOnlyItem("readOnlyFolder"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/child"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/child/test"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/child/readOnlyFile.txt"));
    }

    void testModifyChildItemsInReadOnlyFolder()
    {
        FakeFolder fakeFolder{FileInfo{}};

        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("readOnlyFolder");
        remote.mkdir("readOnlyFolder/test");
        remote.insert("readOnlyFolder/readOnlyFile.txt");

        remote.find("readOnlyFolder")->permissions = RemotePermissions::fromServerString("MG");
        remote.find("readOnlyFolder/test")->permissions = RemotePermissions::fromServerString("mG");
        remote.find("readOnlyFolder/readOnlyFile.txt")->permissions = RemotePermissions::fromServerString("mG");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        remote.insert("readOnlyFolder/test/newFile.txt");
        remote.find("readOnlyFolder/test/newFile.txt")->permissions = RemotePermissions::fromServerString("mG");
        remote.mkdir("readOnlyFolder/test/newFolder");
        remote.find("readOnlyFolder/test/newFolder")->permissions = RemotePermissions::fromServerString("mG");
        remote.appendByte("readOnlyFolder/readOnlyFile.txt");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto ensureReadOnlyItem = [&fakeFolder] (const auto path) -> bool {
            const auto itemStatus = std::filesystem::status(static_cast<QString>(fakeFolder.localPath() + path).toStdWString());
            return static_cast<bool>(itemStatus.permissions() & std::filesystem::perms::owner_read);
        };
        QVERIFY(ensureReadOnlyItem("readOnlyFolder"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/test"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/readOnlyFile.txt"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/test/newFile.txt"));
        QVERIFY(ensureReadOnlyItem("readOnlyFolder/newFolder"));
    }

    void testForbiddenDownload()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QObject parent;

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

            if (op == QNetworkAccessManager::GetOperation) {
                return new FakeErrorReply(op, request, &parent, 403, "Access to this shared resource has been denied because its download permission is disabled.");
            }

            return nullptr;
        });

        fakeFolder.remoteModifier().insert("file");

        auto fileItem = fakeFolder.remoteModifier().find("file");
        Q_ASSERT(fileItem);
        fileItem->isShared = true;
        fileItem->downloadForbidden = true;

        // also hook into discovery!!
        SyncFileItemVector discovery;
        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&discovery](auto v) { discovery = v; });
        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(itemInstruction(completeSpy, "file", CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(discoveryInstruction(discovery, "file", CSYNC_INSTRUCTION_IGNORE));
    }

    void testExistingFileBecomeForbiddenDownload()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QObject parent;

        fakeFolder.remoteModifier().insert("file");
        auto fileInfo = fakeFolder.remoteModifier().find("file");
        Q_ASSERT(fileInfo);
        fileInfo->isShared = true;

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

            if (op == QNetworkAccessManager::GetOperation) {
                return new FakeErrorReply(op, request, &parent, 403, "Access to this shared resource has been denied because its download permission is disabled.");
            }

            return nullptr;
        });

        auto fileItem = fakeFolder.remoteModifier().find("file", FileInfo::Invalidate);
        Q_ASSERT(fileItem);
        fileItem->isShared = true;
        fileItem->downloadForbidden = true;

        // also hook into discovery!!
        SyncFileItemVector discovery;
        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&discovery](auto v) { discovery = v; });
        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(itemInstruction(completeSpy, "file", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(discoveryInstruction(discovery, "file", CSYNC_INSTRUCTION_REMOVE));
    }

    void testChangingPermissionsWithoutEtagChange()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QObject parent;

        fakeFolder.setServerVersion(QStringLiteral("27.0.0"));

        fakeFolder.remoteModifier().mkdir("groupFolder");
        fakeFolder.remoteModifier().mkdir("groupFolder/simpleChildFolder");
        fakeFolder.remoteModifier().insert("groupFolder/simpleChildFolder/otherFile");
        fakeFolder.remoteModifier().mkdir("groupFolder/folderParent");
        fakeFolder.remoteModifier().mkdir("groupFolder/folderParent/childFolder");
        fakeFolder.remoteModifier().insert("groupFolder/folderParent/childFolder/file");

        auto groupFolderRoot = fakeFolder.remoteModifier().find("groupFolder");
        setAllPerm(groupFolderRoot, RemotePermissions::fromServerString("WDNVCKRMG"));

        auto propfindCounter = 0;

        fakeFolder.setServerOverride([&propfindCounter](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

            if (op == QNetworkAccessManager::CustomOperation &&
                request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("PROPFIND")) {
                ++propfindCounter;
            }

            return nullptr;
        });

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(propfindCounter, 5);

        fakeFolder.setServerVersion(QStringLiteral("31.0.0"));

        auto groupFolderRoot2 = fakeFolder.remoteModifier().find("groupFolder");
        groupFolderRoot2->extraDavProperties = "<nc:is-mount-root>true</nc:is-mount-root>";

        fakeFolder.remoteModifier().insert("groupFolder/simpleChildFolder/otherFile", 12);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(propfindCounter, 8);
    }
};

QTEST_GUILESS_MAIN(TestPermissions)
#include "testpermissions.moc"
