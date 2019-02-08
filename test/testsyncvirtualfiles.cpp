/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include "common/vfs.h"
#include <syncengine.h>

using namespace OCC;

SyncFileItemPtr findItem(const QSignalSpy &spy, const QString &path)
{
    for (const QList<QVariant> &args : spy) {
        auto item = args[0].value<SyncFileItemPtr>();
        if (item->destination() == path)
            return item;
    }
    return SyncFileItemPtr(new SyncFileItem);
}

bool itemInstruction(const QSignalSpy &spy, const QString &path, const csync_instructions_e instr)
{
    auto item = findItem(spy, path);
    return item->_instruction == instr;
}

SyncJournalFileRecord dbRecord(FakeFolder &folder, const QString &path)
{
    SyncJournalFileRecord record;
    folder.syncJournal().getFileRecord(path, &record);
    return record;
}

void triggerDownload(FakeFolder &folder, const QByteArray &path)
{
    auto &journal = folder.syncJournal();
    SyncJournalFileRecord record;
    journal.getFileRecord(path + ".owncloud", &record);
    if (!record.isValid())
        return;
    record._type = ItemTypeVirtualFileDownload;
    journal.setFileRecord(record);
    journal.avoidReadFromDbOnNextSync(record._path);
}

void markForDehydration(FakeFolder &folder, const QByteArray &path)
{
    auto &journal = folder.syncJournal();
    SyncJournalFileRecord record;
    journal.getFileRecord(path, &record);
    if (!record.isValid())
        return;
    record._type = ItemTypeVirtualFileDehydration;
    journal.setFileRecord(record);
    journal.avoidReadFromDbOnNextSync(record._path);
}

QSharedPointer<Vfs> setupVfs(FakeFolder &folder)
{
    auto suffixVfs = QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::WithSuffix).release());
    folder.switchToVfs(suffixVfs);

    // Using this directly doesn't recursively unpin everything
    folder.syncJournal().internalPinStates().setForPath("", PinState::OnlineOnly);

    return suffixVfs;
}

class TestSyncVirtualFiles : public QObject
{
    Q_OBJECT

private slots:
    void testVirtualFileLifecycle_data()
    {
        QTest::addColumn<bool>("doLocalDiscovery");

        QTest::newRow("full local discovery") << true;
        QTest::newRow("skip local discovery") << false;
    }

    void testVirtualFileLifecycle()
    {
        QFETCH(bool, doLocalDiscovery);

        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
            if (!doLocalDiscovery)
                fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem);
        };
        cleanup();

        // Create a virtual file for a new remote file
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1", 64);
        auto someDate = QDateTime(QDate(1984, 07, 30), QTime(1,3,2));
        fakeFolder.remoteModifier().setModTime("A/a1", someDate);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1.owncloud").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFile);
        cleanup();

        // Another sync doesn't actually lead to changes
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1.owncloud").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Not even when the remote is rediscovered
        fakeFolder.syncJournal().forceRemoteDiscoveryNextSync();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1.owncloud").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Neither does a remote change
        fakeFolder.remoteModifier().appendByte("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_UPDATE_METADATA));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._fileSize, 65);
        cleanup();

        // If the local virtual file file is removed, it'll just be recreated
        if (!doLocalDiscovery)
            fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, { "A" });
        fakeFolder.localModifier().remove("A/a1.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._fileSize, 65);
        cleanup();

        // Remote rename is propagated
        fakeFolder.remoteModifier().rename("A/a1", "A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1m"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1m.owncloud"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(
            itemInstruction(completeSpy, "A/a1m.owncloud", CSYNC_INSTRUCTION_RENAME)
            || (itemInstruction(completeSpy, "A/a1m.owncloud", CSYNC_INSTRUCTION_NEW)
                && itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_REMOVE)));
        QCOMPARE(dbRecord(fakeFolder, "A/a1m.owncloud")._type, ItemTypeVirtualFile);
        cleanup();

        // Remote remove is propagated
        fakeFolder.remoteModifier().remove("A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1m.owncloud"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(itemInstruction(completeSpy, "A/a1m.owncloud", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a1.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a1m.owncloud").isValid());
        cleanup();

        // Edge case: Local virtual file but no db entry for some reason
        fakeFolder.remoteModifier().insert("A/a2", 64);
        fakeFolder.remoteModifier().insert("A/a3", 64);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3.owncloud"));
        cleanup();

        fakeFolder.syncEngine().journal()->deleteFileRecord("A/a2.owncloud");
        fakeFolder.syncEngine().journal()->deleteFileRecord("A/a3.owncloud");
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(itemInstruction(completeSpy, "A/a2.owncloud", CSYNC_INSTRUCTION_UPDATE_METADATA));
        QVERIFY(dbRecord(fakeFolder, "A/a2.owncloud").isValid());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a3.owncloud"));
        QVERIFY(itemInstruction(completeSpy, "A/a3.owncloud", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a3.owncloud").isValid());
        cleanup();
    }

    void testVirtualFileConflict()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // Create a virtual file for a new remote file
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1", 64);
        fakeFolder.remoteModifier().insert("A/a2", 64);
        fakeFolder.remoteModifier().mkdir("B");
        fakeFolder.remoteModifier().insert("B/b1", 64);
        fakeFolder.remoteModifier().insert("B/b2", 64);
        fakeFolder.remoteModifier().mkdir("C");
        fakeFolder.remoteModifier().insert("C/c1", 64);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/b2.owncloud"));
        cleanup();

        // A: the correct file and a conflicting file are added, virtual files stay
        // B: same setup, but the virtual files are deleted by the user
        // C: user adds a *directory* locally
        fakeFolder.localModifier().insert("A/a1", 64);
        fakeFolder.localModifier().insert("A/a2", 30);
        fakeFolder.localModifier().insert("B/b1", 64);
        fakeFolder.localModifier().insert("B/b2", 30);
        fakeFolder.localModifier().remove("B/b1.owncloud");
        fakeFolder.localModifier().remove("B/b2.owncloud");
        fakeFolder.localModifier().mkdir("C/c1");
        fakeFolder.localModifier().insert("C/c1/foo");
        QVERIFY(fakeFolder.syncOnce());

        // Everything is CONFLICT since mtimes are different even for a1/b1
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "B/b1", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "B/b2", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "C/c1", CSYNC_INSTRUCTION_CONFLICT));

        // no virtual file files should remain
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b1.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("C/c1.owncloud"));

        // conflict files should exist
        QCOMPARE(fakeFolder.syncJournal().conflictRecordPaths().size(), 3);

        // nothing should have the virtual file tag
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "C/c1")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a2.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "B/b1.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "B/b2.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "C/c1.owncloud").isValid());

        cleanup();
    }

    void testWithNormalSync()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // No effect sync
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        cleanup();

        // Existing files are propagated just fine in both directions
        fakeFolder.localModifier().appendByte("A/a1");
        fakeFolder.localModifier().insert("A/a3");
        fakeFolder.remoteModifier().appendByte("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        cleanup();

        // New files on the remote create virtual files
        fakeFolder.remoteModifier().insert("A/new");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/new"));
        QVERIFY(fakeFolder.currentLocalState().find("A/new.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/new"));
        QVERIFY(itemInstruction(completeSpy, "A/new.owncloud", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/new.owncloud")._type, ItemTypeVirtualFile);
        cleanup();
    }

    void testVirtualFileDownload()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        fakeFolder.remoteModifier().insert("A/a2");
        fakeFolder.remoteModifier().insert("A/a3");
        fakeFolder.remoteModifier().insert("A/a4");
        fakeFolder.remoteModifier().insert("A/a5");
        fakeFolder.remoteModifier().insert("A/a6");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a4.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a5.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a6.owncloud"));
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, "A/a1");
        triggerDownload(fakeFolder, "A/a2");
        triggerDownload(fakeFolder, "A/a3");
        triggerDownload(fakeFolder, "A/a4");
        triggerDownload(fakeFolder, "A/a5");
        triggerDownload(fakeFolder, "A/a6");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.remoteModifier().rename("A/a4", "A/a4m");
        fakeFolder.localModifier().insert("A/a5");
        fakeFolder.localModifier().insert("A/a6");
        fakeFolder.localModifier().remove("A/a6.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a2.owncloud", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "A/a3.owncloud", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a4m", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a4.owncloud", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a5", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a5.owncloud", CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "A/a6", CSYNC_INSTRUCTION_CONFLICT));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a3").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/a4m")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a5")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a6")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a2.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a3.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a4.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a5.owncloud").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a6.owncloud").isValid());
    }

    void testVirtualFileDownloadResume()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
            fakeFolder.syncJournal().wipeErrorBlacklist();
        };
        cleanup();

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, "A/a1");
        fakeFolder.serverErrorPaths().append("A/a1", 500);
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_NONE));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFileDownload);
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        cleanup();

        fakeFolder.serverErrorPaths().clear();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_NONE));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1.owncloud").isValid());
    }

    void testNewFilesNotVirtual()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));

        fakeFolder.syncJournal().internalPinStates().setForPath("", PinState::AlwaysLocal);

        // Create a new remote file, it'll not be virtual
        fakeFolder.remoteModifier().insert("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2.owncloud"));
    }

    void testDownloadRecursive()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().mkdir("A/Sub");
        fakeFolder.remoteModifier().mkdir("A/Sub/SubSub");
        fakeFolder.remoteModifier().mkdir("A/Sub2");
        fakeFolder.remoteModifier().mkdir("B");
        fakeFolder.remoteModifier().mkdir("B/Sub");
        fakeFolder.remoteModifier().insert("A/a1");
        fakeFolder.remoteModifier().insert("A/a2");
        fakeFolder.remoteModifier().insert("A/Sub/a3");
        fakeFolder.remoteModifier().insert("A/Sub/a4");
        fakeFolder.remoteModifier().insert("A/Sub/SubSub/a5");
        fakeFolder.remoteModifier().insert("A/Sub2/a6");
        fakeFolder.remoteModifier().insert("B/b1");
        fakeFolder.remoteModifier().insert("B/Sub/b2");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a3.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a4.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a5.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub2/a6.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/b1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/Sub/b2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a3"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a4"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a5"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub2/a6"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b1"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/Sub/b2"));


        // Download All file in the directory A/Sub
        // (as in Folder::downloadVirtualFile)
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A/Sub");

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a3.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a4.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a5.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub2/a6.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/b1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/Sub/b2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a3"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a4"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a5"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub2/a6"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b1"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/Sub/b2"));

        // Add a file in a subfolder that was downloaded
        // Currently, this continue to add it as a virtual file.
        fakeFolder.remoteModifier().insert("A/Sub/SubSub/a7");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a7.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a7"));

        // Now download all files in "A"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a3.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a4.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a5.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub2/a6.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a7.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/b1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("B/Sub/b2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a3"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a4"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a5"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub2/a6"));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a7"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b1"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/Sub/b2"));

        // Now download remaining files in "B"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("B");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRenameToVirtual()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // If a file is renamed to <name>.owncloud, it becomes virtual
        fakeFolder.localModifier().rename("A/a1", "A/a1.owncloud");
        // If a file is renamed to <random>.owncloud, the file sticks around (to preserve user data)
        fakeFolder.localModifier().rename("A/a2", "A/rand.owncloud");
        // dangling virtual files are removed
        fakeFolder.localModifier().insert("A/dangling.owncloud", 1, ' ');
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1.owncloud")._type, ItemTypeVirtualFile);

        QVERIFY(!fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/rand.owncloud"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a2"));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/rand.owncloud").isValid());

        QVERIFY(!fakeFolder.currentLocalState().find("A/dangling.owncloud"));

        cleanup();
    }

    void testRenameVirtual()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        fakeFolder.remoteModifier().insert("file1", 128, 'C');
        fakeFolder.remoteModifier().insert("file2", 256, 'C');
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("file1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("file2.owncloud"));
        cleanup();

        fakeFolder.localModifier().rename("file1.owncloud", "renamed1.owncloud");
        fakeFolder.localModifier().rename("file2.owncloud", "renamed2.owncloud");
        triggerDownload(fakeFolder, "file2");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.currentLocalState().find("file1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("renamed1.owncloud"));
        QVERIFY(!fakeFolder.currentRemoteState().find("file1"));
        QVERIFY(fakeFolder.currentRemoteState().find("renamed1"));
        QVERIFY(itemInstruction(completeSpy, "renamed1.owncloud", CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "renamed1.owncloud").isValid());

        // file2 has a conflict between the download request and the rename:
        // currently the download wins
        QVERIFY(!fakeFolder.currentLocalState().find("file2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("file2"));
        QVERIFY(fakeFolder.currentRemoteState().find("file2"));
        QVERIFY(itemInstruction(completeSpy, "file2", CSYNC_INSTRUCTION_NEW));
        QVERIFY(dbRecord(fakeFolder, "file2").isValid());
    }

    // Dehydration via sync works
    void testSyncDehydration()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        setupVfs(fakeFolder);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        //
        // Mark for dehydration and check
        //

        markForDehydration(fakeFolder, "A/a1");

        markForDehydration(fakeFolder, "A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        // expect: normal dehydration

        markForDehydration(fakeFolder, "B/b1");
        fakeFolder.remoteModifier().remove("B/b1");
        // expect: local removal

        markForDehydration(fakeFolder, "B/b2");
        fakeFolder.remoteModifier().rename("B/b2", "B/b3");
        // expect: B/b2 is gone, B/b3 is NEW placeholder

        markForDehydration(fakeFolder, "C/c1");
        fakeFolder.localModifier().appendByte("C/c1");
        // expect: no dehydration, upload of c1

        markForDehydration(fakeFolder, "C/c2");
        fakeFolder.localModifier().appendByte("C/c2");
        fakeFolder.remoteModifier().appendByte("C/c2");
        fakeFolder.remoteModifier().appendByte("C/c2");
        // expect: no dehydration, conflict

        QVERIFY(fakeFolder.syncOnce());

        auto isDehydrated = [&](const QString &path) {
            QString placeholder = path + ".owncloud";
            return !fakeFolder.currentLocalState().find(path)
                && fakeFolder.currentLocalState().find(placeholder);
        };
        auto hasDehydratedDbEntries = [&](const QString &path) {
            SyncJournalFileRecord normal, suffix;
            fakeFolder.syncJournal().getFileRecord(path, &normal);
            fakeFolder.syncJournal().getFileRecord(path + ".owncloud", &suffix);
            return !normal.isValid() && suffix.isValid() && suffix._type == ItemTypeVirtualFile;
        };

        QVERIFY(isDehydrated("A/a1"));
        QVERIFY(hasDehydratedDbEntries("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1.owncloud", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(isDehydrated("A/a2"));
        QVERIFY(hasDehydratedDbEntries("A/a2"));
        QVERIFY(itemInstruction(completeSpy, "A/a2.owncloud", CSYNC_INSTRUCTION_SYNC));

        QVERIFY(!fakeFolder.currentLocalState().find("B/b1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(itemInstruction(completeSpy, "B/b1", CSYNC_INSTRUCTION_REMOVE));

        QVERIFY(!fakeFolder.currentLocalState().find("B/b2"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b2"));
        QVERIFY(isDehydrated("B/b3"));
        QVERIFY(hasDehydratedDbEntries("B/b3"));
        QVERIFY(itemInstruction(completeSpy, "B/b2", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "B/b3.owncloud", CSYNC_INSTRUCTION_NEW));

        QCOMPARE(fakeFolder.currentRemoteState().find("C/c1")->size, 25);
        QVERIFY(itemInstruction(completeSpy, "C/c1", CSYNC_INSTRUCTION_SYNC));

        QCOMPARE(fakeFolder.currentRemoteState().find("C/c2")->size, 26);
        QVERIFY(itemInstruction(completeSpy, "C/c2", CSYNC_INSTRUCTION_CONFLICT));
        cleanup();

        auto expectedLocalState = fakeFolder.currentLocalState();
        auto expectedRemoteState = fakeFolder.currentRemoteState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedLocalState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedRemoteState);
    }

    void testWipeVirtualSuffixFiles()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder);

        // Create a suffix-vfs baseline

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().mkdir("A/B");
        fakeFolder.remoteModifier().insert("f1");
        fakeFolder.remoteModifier().insert("A/a1");
        fakeFolder.remoteModifier().insert("A/a3");
        fakeFolder.remoteModifier().insert("A/B/b1");
        fakeFolder.localModifier().mkdir("A");
        fakeFolder.localModifier().mkdir("A/B");
        fakeFolder.localModifier().insert("f2");
        fakeFolder.localModifier().insert("A/a2");
        fakeFolder.localModifier().insert("A/B/b2");

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("f1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/B/b1.owncloud"));

        // Make local changes to a3
        fakeFolder.localModifier().remove("A/a3.owncloud");
        fakeFolder.localModifier().insert("A/a3.owncloud", 100);

        // Now wipe the virtuals

        SyncEngine::wipeVirtualFiles(fakeFolder.localPath(), fakeFolder.syncJournal(), *fakeFolder.syncEngine().syncOptions()._vfs);

        QVERIFY(!fakeFolder.currentLocalState().find("f1.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/B/b1.owncloud"));

        fakeFolder.switchToVfs(QSharedPointer<Vfs>(new VfsOff));
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("A/a3.owncloud")); // regular upload
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testNewVirtuals()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        auto setPin = [&] (const QByteArray &path, PinState state) {
            fakeFolder.syncJournal().internalPinStates().setForPath(path, state);
        };

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("online");
        fakeFolder.remoteModifier().mkdir("unspec");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPin("local", PinState::AlwaysLocal);
        setPin("online", PinState::OnlineOnly);
        setPin("unspec", PinState::Unspecified);

        // Test 1: root is OnlineOnly
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("online/file1");
        fakeFolder.remoteModifier().insert("local/file1");
        fakeFolder.remoteModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("file1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("online/file1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file1.owncloud"));

        // Test 2: root is AlwaysLocal
        setPin("", PinState::AlwaysLocal);

        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("online/file2");
        fakeFolder.remoteModifier().insert("local/file2");
        fakeFolder.remoteModifier().insert("unspec/file2");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("file2"));
        QVERIFY(fakeFolder.currentLocalState().find("online/file2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("local/file2"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file2.owncloud"));

        // file1 is unchanged
        QVERIFY(fakeFolder.currentLocalState().find("file1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("online/file1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file1.owncloud"));
    }

    // Check what happens if vfs-suffixed files exist on the server or in the db
    void testSuffixOnServerOrDb()
    {
        FakeFolder fakeFolder{ FileInfo() };

        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // file1.owncloud is happily synced with Vfs::Off
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/file1.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        cleanup();

        // Enable suffix vfs
        setupVfs(fakeFolder);

        // Local changes of suffixed file do nothing
        fakeFolder.localModifier().appendByte("A/file1.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1.owncloud", CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // Remote don't do anything either
        fakeFolder.remoteModifier().appendByte("A/file1.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1.owncloud", CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // New files with a suffix aren't propagated downwards in the first place
        fakeFolder.remoteModifier().insert("A/file2.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file2.owncloud", CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(fakeFolder.currentRemoteState().find("A/file2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file2"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file2.owncloud"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file2.owncloud.owncloud"));
        cleanup();
    }
};

QTEST_GUILESS_MAIN(TestSyncVirtualFiles)
#include "testsyncvirtualfiles.moc"
