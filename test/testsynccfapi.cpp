/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include "common/vfs.h"
#include "config.h"
#include <syncengine.h>

#include "vfs/cfapi/cfapiwrapper.h"

namespace cfapi {
using namespace OCC::CfApiWrapper;
}

using namespace OCC;

void setPinState(const QString &path, PinState state, cfapi::SetPinRecurseMode mode)
{
    Q_ASSERT(mode == cfapi::Recurse || mode == cfapi::NoRecurse);

    const auto p = QDir::toNativeSeparators(path);
    const auto handle = cfapi::handleForPath(p);
    Q_ASSERT(handle);

    const auto result = cfapi::setPinState(handle, state, mode);
    Q_ASSERT(result);

    if (mode == cfapi::NoRecurse) {
        const auto result = cfapi::setPinState(handle, PinState::Inherited, cfapi::ChildrenOnly);
        Q_ASSERT(result);
    }
}

bool itemInstruction(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
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
    journal.getFileRecord(path, &record);
    if (!record.isValid())
        return;
    record._type = ItemTypeVirtualFileDownload;
    journal.setFileRecord(record);
    journal.schedulePathForRemoteDiscovery(record._path);
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
    journal.schedulePathForRemoteDiscovery(record._path);
}

QSharedPointer<Vfs> setupVfs(FakeFolder &folder)
{
    auto cfapiVfs = QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::WindowsCfApi).release());
    QObject::connect(&folder.syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
                     cfapiVfs.data(), &Vfs::fileStatusChanged);
    folder.switchToVfs(cfapiVfs);

    setPinState(folder.localPath(), PinState::Unspecified, cfapi::NoRecurse);

    return cfapiVfs;
}

class TestSyncCfApi : public QObject
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
        ItemCompletedSpy completeSpy(fakeFolder);

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
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").size(), 64);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        cleanup();

        // Another sync doesn't actually lead to changes
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").size(), 64);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Not even when the remote is rediscovered
        fakeFolder.syncJournal().forceRemoteDiscoveryNextSync();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").size(), 64);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Neither does a remote change
        fakeFolder.remoteModifier().appendByte("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").size(), 65);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_UPDATE_METADATA));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._fileSize, 65);
        cleanup();

        // If the local virtual file is removed, this will be propagated remotely
        if (!doLocalDiscovery)
            fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, { "A" });
        fakeFolder.localModifier().remove("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        cleanup();

        // Recreate a1 before carrying on with the other tests
        fakeFolder.remoteModifier().insert("A/a1", 65);
        fakeFolder.remoteModifier().setModTime("A/a1", someDate);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").size(), 65);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        cleanup();

        // Remote rename is propagated
        fakeFolder.remoteModifier().rename("A/a1", "A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1m"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1m").size(), 65);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1m").lastModified(), someDate);
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(
            itemInstruction(completeSpy, "A/a1m", CSYNC_INSTRUCTION_RENAME)
            || (itemInstruction(completeSpy, "A/a1m", CSYNC_INSTRUCTION_NEW)
                && itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_REMOVE)));
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/a1m")._type, ItemTypeVirtualFile);
        cleanup();

        // Remote remove is propagated
        fakeFolder.remoteModifier().remove("A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/a1m").exists());
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(itemInstruction(completeSpy, "A/a1m", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a1m").isValid());
        cleanup();

        // Edge case: Local virtual file but no db entry for some reason
        fakeFolder.remoteModifier().insert("A/a2", 32);
        fakeFolder.remoteModifier().insert("A/a3", 33);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a2").size(), 32);
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a3"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a3").size(), 33);
        cleanup();

        fakeFolder.syncEngine().journal()->deleteFileRecord("A/a2");
        fakeFolder.syncEngine().journal()->deleteFileRecord("A/a3");
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a2").size(), 32);
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_UPDATE_METADATA));
        QVERIFY(dbRecord(fakeFolder, "A/a2").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeVirtualFile);
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/a3").exists());
        QVERIFY(itemInstruction(completeSpy, "A/a3", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a3").isValid());
        cleanup();
    }

    void testVirtualFileConflict()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // Create a virtual file for a new remote file
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1", 11);
        fakeFolder.remoteModifier().insert("A/a2", 12);
        fakeFolder.remoteModifier().mkdir("B");
        fakeFolder.remoteModifier().insert("B/b1", 21);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/b1"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").size(), 11);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a2").size(), 12);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "B/b1").size(), 21);
        cleanup();

        // All the files are touched on the server
        fakeFolder.remoteModifier().appendByte("A/a1");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("B/b1");

        // A: the correct file and a conflicting file are added
        // B: user adds a *directory* locally
        fakeFolder.localModifier().remove("A/a1");
        fakeFolder.localModifier().insert("A/a1", 12);
        fakeFolder.localModifier().remove("A/a2");
        fakeFolder.localModifier().insert("A/a2", 10);
        fakeFolder.localModifier().remove("B/b1");
        fakeFolder.localModifier().mkdir("B/b1");
        fakeFolder.localModifier().insert("B/b1/foo");
        QVERIFY(fakeFolder.syncOnce());

        // Everything is CONFLICT
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "B/b1", CSYNC_INSTRUCTION_CONFLICT));

        // conflict files should exist
        QCOMPARE(fakeFolder.syncJournal().conflictRecordPaths().size(), 2);

        // nothing should have the virtual file tag
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "B/b1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b1")._type, ItemTypeFile);

        cleanup();
    }

    void testWithNormalSync()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

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
        fakeFolder.remoteModifier().insert("A/new", 42);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/new"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/new").size(), 42);
        QVERIFY(fakeFolder.currentRemoteState().find("A/new"));
        QVERIFY(itemInstruction(completeSpy, "A/new", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/new")._type, ItemTypeVirtualFile);
        cleanup();
    }

    void testVirtualFileDownload()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

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
        fakeFolder.remoteModifier().insert("A/a7");
        fakeFolder.remoteModifier().insert("A/b1");
        fakeFolder.remoteModifier().insert("A/b2");
        fakeFolder.remoteModifier().insert("A/b3");
        fakeFolder.remoteModifier().insert("A/b4");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a3"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a4"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a5"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a6"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a7"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/b1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/b2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/b3"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/b4"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a4").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a5").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a6").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a7").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/b1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/b2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/b3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/b4").exists());

        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a4")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a5")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a6")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a7")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b2")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b4")._type, ItemTypeVirtualFile);

        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, "A/a1");
        triggerDownload(fakeFolder, "A/a2");
        triggerDownload(fakeFolder, "A/a3");
        triggerDownload(fakeFolder, "A/a4");
        triggerDownload(fakeFolder, "A/a5");
        triggerDownload(fakeFolder, "A/a6");
        triggerDownload(fakeFolder, "A/a7");
        triggerDownload(fakeFolder, "A/b1");
        triggerDownload(fakeFolder, "A/b2");
        triggerDownload(fakeFolder, "A/b3");
        triggerDownload(fakeFolder, "A/b4");

        // Remote complications
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.remoteModifier().rename("A/a4", "A/a4m");
        fakeFolder.remoteModifier().appendByte("A/b2");
        fakeFolder.remoteModifier().remove("A/b3");
        fakeFolder.remoteModifier().rename("A/b4", "A/b4m");

        // Local complications
        fakeFolder.localModifier().remove("A/a5");
        fakeFolder.localModifier().insert("A/a5");
        fakeFolder.localModifier().remove("A/a6");
        fakeFolder.localModifier().insert("A/a6");

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a1")->_type, ItemTypeVirtualFileDownload);
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a2")->_type, ItemTypeVirtualFileDownload);
        QVERIFY(itemInstruction(completeSpy, "A/a3", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a4m", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a4", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a5", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a6", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a7", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/b1", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/b2", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/b3", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/b4m", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/b4", CSYNC_INSTRUCTION_REMOVE));

        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/a3").exists());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/a4").exists());
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a4m"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a5"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a6"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a7"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/b1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/b2"));
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/b3").exists());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/b4").exists());
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/b4m"));

        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a3").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/a4m")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a5")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a6")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a7")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b2")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/b3").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/b4m")._type, ItemTypeFile);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testVirtualFileDownloadResume()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
            fakeFolder.syncJournal().wipeErrorBlacklist();
        };
        cleanup();

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, "A/a1");
        fakeFolder.serverErrorPaths().append("A/a1", 500);
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFileDownload);
        cleanup();

        fakeFolder.serverErrorPaths().clear();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
    }

    void testNewFilesNotVirtual()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);

        setPinState(fakeFolder.localPath(), PinState::AlwaysLocal, cfapi::NoRecurse);

        // Create a new remote file, it'll not be virtual
        fakeFolder.remoteModifier().insert("A/a2");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a2").exists());
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
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

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/a3"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/a4"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/SubSub/a5"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub2/a6"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/b1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/Sub/b2"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/a3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/a4").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/SubSub/a5").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub2/a6").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "B/b1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "B/Sub/b2").exists());

        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/a3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/a4")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/SubSub/a5")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub2/a6")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "B/Sub/b2")._type, ItemTypeVirtualFile);

        // Download All file in the directory A/Sub
        // (as in Folder::downloadVirtualFile)
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A/Sub");

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/a3"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/a4"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/SubSub/a5"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub2/a6"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/b1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/Sub/b2"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/a3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/a4").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/SubSub/a5").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub2/a6").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "B/b1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "B/Sub/b2").exists());

        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/a3")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/a4")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/SubSub/a5")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub2/a6")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "B/Sub/b2")._type, ItemTypeVirtualFile);

        // Add a file in a subfolder that was downloaded
        // Currently, this continue to add it as a virtual file.
        fakeFolder.remoteModifier().insert("A/Sub/SubSub/a7");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/SubSub/a7"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/SubSub/a7").exists());
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/SubSub/a7")._type, ItemTypeVirtualFile);

        // Now download all files in "A"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/a2"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/a3"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/a4"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/SubSub/a5"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub/SubSub/a7"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "A/Sub2/a6"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/b1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "B/Sub/b2"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/a3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/a4").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/SubSub/a5").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub/SubSub/a7").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/Sub2/a6").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "B/b1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "B/Sub/b2").exists());

        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/a3")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/a4")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/SubSub/a5")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub/SubSub/a7")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/Sub2/a6")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "B/Sub/b2")._type, ItemTypeVirtualFile);

        // Now download remaining files in "B"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("B");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRenameVirtual()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        fakeFolder.remoteModifier().insert("file1", 128, 'C');
        fakeFolder.remoteModifier().insert("file2", 256, 'C');
        fakeFolder.remoteModifier().insert("file3", 256, 'C');
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "file1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "file2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "file3"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "file2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "file3").exists());

        QCOMPARE(dbRecord(fakeFolder, "file1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "file2")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "file3")._type, ItemTypeVirtualFile);

        cleanup();

        fakeFolder.localModifier().rename("file1", "renamed1");
        fakeFolder.localModifier().rename("file2", "renamed2");
        triggerDownload(fakeFolder, "file2");
        triggerDownload(fakeFolder, "file3");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!QFileInfo(fakeFolder.localPath() + "file1").exists());
        QVERIFY(!dbRecord(fakeFolder, "file1").isValid());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "renamed1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "renamed1").exists());
        QCOMPARE(dbRecord(fakeFolder, "renamed1")._type, ItemTypeVirtualFile);

        QVERIFY(fakeFolder.currentRemoteState().find("renamed1"));
        QVERIFY(itemInstruction(completeSpy, "renamed1", CSYNC_INSTRUCTION_RENAME));

        // file2 has a conflict between the download request and the rename:
        // the rename wins, the download is ignored

        QVERIFY(!QFileInfo(fakeFolder.localPath() + "file2").exists());
        QVERIFY(!dbRecord(fakeFolder, "file2").isValid());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "renamed2"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "renamed2").exists());
        QCOMPARE(dbRecord(fakeFolder, "renamed2")._type, ItemTypeVirtualFile);

        QVERIFY(fakeFolder.currentRemoteState().find("renamed2"));
        QVERIFY(itemInstruction(completeSpy, "renamed2", CSYNC_INSTRUCTION_RENAME));

        QVERIFY(itemInstruction(completeSpy, "file3", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "file3"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "file3").exists());
        QVERIFY(dbRecord(fakeFolder, "file3")._type == ItemTypeFile);
        cleanup();
    }

    void testRenameVirtual2()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        ItemCompletedSpy completeSpy(fakeFolder);
        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        fakeFolder.remoteModifier().insert("case3", 128, 'C');
        fakeFolder.remoteModifier().insert("case4", 256, 'C');
        QVERIFY(fakeFolder.syncOnce());

        triggerDownload(fakeFolder, "case4");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "case3"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "case4"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "case3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "case4").exists());

        QCOMPARE(dbRecord(fakeFolder, "case3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "case4")._type, ItemTypeFile);

        cleanup();

        // Case 1: non-virtual, foo -> bar (tested elsewhere)
        // Case 2: virtual, foo -> bar (tested elsewhere)

        // Case 3: virtual, foo.oc -> bar.oc (db hydrate)
        fakeFolder.localModifier().rename("case3", "case3-rename");
        triggerDownload(fakeFolder, "case3");

        // Case 4: non-virtual foo -> bar (db dehydrate)
        fakeFolder.localModifier().rename("case4", "case4-rename");
        markForDehydration(fakeFolder, "case4");

        QVERIFY(fakeFolder.syncOnce());

        // Case 3: the rename went though, hydration is forgotten
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "case3").exists());
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "case3-rename"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "case3-rename").exists());
        QVERIFY(!fakeFolder.currentRemoteState().find("case3"));
        QVERIFY(fakeFolder.currentRemoteState().find("case3-rename"));
        QVERIFY(itemInstruction(completeSpy, "case3-rename", CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "case3-rename")._type == ItemTypeVirtualFile);

        // Case 4: the rename went though, dehydration is forgotten
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "case4").exists());
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "case4-rename"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "case4-rename").exists());
        QVERIFY(!fakeFolder.currentRemoteState().find("case4"));
        QVERIFY(fakeFolder.currentRemoteState().find("case4-rename"));
        QVERIFY(itemInstruction(completeSpy, "case4-rename", CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "case4-rename")._type == ItemTypeFile);
    }

    // Dehydration via sync works
    void testSyncDehydration()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        setupVfs(fakeFolder);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        ItemCompletedSpy completeSpy(fakeFolder);
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
            return cfapi::isSparseFile(fakeFolder.localPath() + path)
                && QFileInfo(fakeFolder.localPath() + path).exists();
        };
        auto hasDehydratedDbEntries = [&](const QString &path) {
            SyncJournalFileRecord rec;
            fakeFolder.syncJournal().getFileRecord(path, &rec);
            return rec.isValid() && rec._type == ItemTypeVirtualFile;
        };

        QVERIFY(isDehydrated("A/a1"));
        QVERIFY(hasDehydratedDbEntries("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a1")->_type, ItemTypeVirtualFileDehydration);
        QCOMPARE(completeSpy.findItem("A/a1")->_file, QStringLiteral("A/a1"));
        QVERIFY(isDehydrated("A/a2"));
        QVERIFY(hasDehydratedDbEntries("A/a2"));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a2")->_type, ItemTypeVirtualFileDehydration);

        QVERIFY(!QFileInfo(fakeFolder.localPath() + "B/b1").exists());
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(itemInstruction(completeSpy, "B/b1", CSYNC_INSTRUCTION_REMOVE));

        QVERIFY(!QFileInfo(fakeFolder.localPath() + "B/b2").exists());
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b2"));
        QVERIFY(isDehydrated("B/b3"));
        QVERIFY(hasDehydratedDbEntries("B/b3"));
        QVERIFY(itemInstruction(completeSpy, "B/b2", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "B/b3", CSYNC_INSTRUCTION_NEW));

        QCOMPARE(fakeFolder.currentRemoteState().find("C/c1")->size, 25);
        QVERIFY(itemInstruction(completeSpy, "C/c1", CSYNC_INSTRUCTION_SYNC));

        QCOMPARE(fakeFolder.currentRemoteState().find("C/c2")->size, 26);
        QVERIFY(itemInstruction(completeSpy, "C/c2", CSYNC_INSTRUCTION_CONFLICT));
        cleanup();

        auto expectedRemoteState = fakeFolder.currentRemoteState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentRemoteState(), expectedRemoteState);

        QVERIFY(isDehydrated("A/a1"));
        QVERIFY(hasDehydratedDbEntries("A/a1"));
        QVERIFY(isDehydrated("A/a2"));
        QVERIFY(hasDehydratedDbEntries("A/a2"));

        QVERIFY(!QFileInfo(fakeFolder.localPath() + "B/b1").exists());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "B/b2").exists());
        QVERIFY(isDehydrated("B/b3"));
        QVERIFY(hasDehydratedDbEntries("B/b3"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "C/c1").exists());
        QVERIFY(dbRecord(fakeFolder, "C/c1").isValid());
        QVERIFY(!isDehydrated("C/c1"));
        QVERIFY(!hasDehydratedDbEntries("C/c1"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "C/c2").exists());
        QVERIFY(dbRecord(fakeFolder, "C/c2").isValid());
        QVERIFY(!isDehydrated("C/c2"));
        QVERIFY(!hasDehydratedDbEntries("C/c2"));
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

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "f1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/a3"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "A/B/b1"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "f1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/B/b1").exists());

        QCOMPARE(dbRecord(fakeFolder, "f1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/B/b1")._type, ItemTypeVirtualFile);

        // Make local changes to a3
        fakeFolder.localModifier().remove("A/a3");
        fakeFolder.localModifier().insert("A/a3", 100);

        // Now wipe the virtuals
        SyncEngine::wipeVirtualFiles(fakeFolder.localPath(), fakeFolder.syncJournal(), *fakeFolder.syncEngine().syncOptions()._vfs);

        QVERIFY(!QFileInfo(fakeFolder.localPath() + "f1").exists());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/a1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "A/a3").exists());
        QVERIFY(!QFileInfo(fakeFolder.localPath() + "A/B/b1").exists());

        QVERIFY(!dbRecord(fakeFolder, "f1").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a3").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/B/b1").isValid());

        fakeFolder.switchToVfs(QSharedPointer<Vfs>(new VfsOff));
        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("A"));
        QVERIFY(fakeFolder.currentLocalState().find("A/B"));
        QVERIFY(fakeFolder.currentLocalState().find("A/B/b1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/B/b2"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3"));
        QVERIFY(fakeFolder.currentLocalState().find("f1"));
        QVERIFY(fakeFolder.currentLocalState().find("f2"));

        // a3 has a conflict
        QVERIFY(itemInstruction(completeSpy, "A/a3", CSYNC_INSTRUCTION_CONFLICT));

        // conflict files should exist
        QCOMPARE(fakeFolder.syncJournal().conflictRecordPaths().size(), 1);
    }

    void testNewVirtuals()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("online");
        fakeFolder.remoteModifier().mkdir("unspec");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPinState(fakeFolder.localPath() + "local", PinState::AlwaysLocal, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "online", PinState::OnlineOnly, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "unspec", PinState::Unspecified, cfapi::Recurse);

        // Test 1: root is Unspecified
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("online/file1");
        fakeFolder.remoteModifier().insert("local/file1");
        fakeFolder.remoteModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "file1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "online/file1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "local/file1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "unspec/file1"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "unspec/file1").exists());

        QCOMPARE(dbRecord(fakeFolder, "file1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "online/file1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "local/file1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "unspec/file1")._type, ItemTypeVirtualFile);

        // Test 2: change root to AlwaysLocal
        setPinState(fakeFolder.localPath(), PinState::AlwaysLocal, cfapi::Recurse);
        // Need to force pin state for the subfolders again
        setPinState(fakeFolder.localPath() + "local", PinState::AlwaysLocal, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "online", PinState::OnlineOnly, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "unspec", PinState::Unspecified, cfapi::Recurse);

        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("online/file2");
        fakeFolder.remoteModifier().insert("local/file2");
        fakeFolder.remoteModifier().insert("unspec/file2");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "file2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "online/file2"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "local/file2"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "unspec/file2"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "file2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file2").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "unspec/file2").exists());

        QCOMPARE(dbRecord(fakeFolder, "file2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "online/file2")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "local/file2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "unspec/file2")._type, ItemTypeVirtualFile);

        // root file1 was hydrated due to its new pin state
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "file1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "file1").exists());
        QCOMPARE(dbRecord(fakeFolder, "file1")._type, ItemTypeFile);

        // file1 is unchanged in the explicitly pinned subfolders
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "online/file1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "local/file1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "unspec/file1"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "unspec/file1").exists());

        QCOMPARE(dbRecord(fakeFolder, "online/file1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "local/file1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "unspec/file1")._type, ItemTypeVirtualFile);

        // Test 3: change root to OnlineOnly
        setPinState(fakeFolder.localPath(), PinState::OnlineOnly, cfapi::Recurse);
        // Need to force pin state for the subfolders again
        setPinState(fakeFolder.localPath() + "local", PinState::AlwaysLocal, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "online", PinState::OnlineOnly, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "unspec", PinState::Unspecified, cfapi::Recurse);

        fakeFolder.remoteModifier().insert("file3");
        fakeFolder.remoteModifier().insert("online/file3");
        fakeFolder.remoteModifier().insert("local/file3");
        fakeFolder.remoteModifier().insert("unspec/file3");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "file3"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "online/file3"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "local/file3"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "unspec/file3"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "file3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file3").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "unspec/file3").exists());

        QCOMPARE(dbRecord(fakeFolder, "file3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "online/file3")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "local/file3")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "unspec/file3")._type, ItemTypeVirtualFile);


        // root file1 was dehydrated due to its new pin state
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "file1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "file1").exists());
        QCOMPARE(dbRecord(fakeFolder, "file1")._type, ItemTypeVirtualFile);

        // file1 is unchanged in the explicitly pinned subfolders
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "online/file1"));
        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "local/file1"));
        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "unspec/file1"));

        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file1").exists());
        QVERIFY(QFileInfo(fakeFolder.localPath() + "unspec/file1").exists());

        QCOMPARE(dbRecord(fakeFolder, "online/file1")._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "local/file1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "unspec/file1")._type, ItemTypeVirtualFile);
    }

    void testAvailability()
    {
        FakeFolder fakeFolder{ FileInfo() };
        auto vfs = setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("local/sub");
        fakeFolder.remoteModifier().mkdir("online");
        fakeFolder.remoteModifier().mkdir("online/sub");
        fakeFolder.remoteModifier().mkdir("unspec");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPinState(fakeFolder.localPath() + "local", PinState::AlwaysLocal, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "online", PinState::OnlineOnly, cfapi::Recurse);
        setPinState(fakeFolder.localPath() + "unspec", PinState::Unspecified, cfapi::Recurse);

        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("online/file1");
        fakeFolder.remoteModifier().insert("online/file2");
        fakeFolder.remoteModifier().insert("local/file1");
        fakeFolder.remoteModifier().insert("local/file2");
        fakeFolder.remoteModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());

        // root is unspecified
        QCOMPARE(*vfs->availability("file1"), VfsItemAvailability::AllDehydrated);
        QCOMPARE(*vfs->availability("local"), VfsItemAvailability::AlwaysLocal);
        QCOMPARE(*vfs->availability("local/file1"), VfsItemAvailability::AlwaysLocal);
        QCOMPARE(*vfs->availability("online"), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability("online/file1"), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability("unspec"), VfsItemAvailability::AllDehydrated);
        QCOMPARE(*vfs->availability("unspec/file1"), VfsItemAvailability::AllDehydrated);

        // Subitem pin states can ruin "pure" availabilities
        setPinState(fakeFolder.localPath() + "local/sub", PinState::OnlineOnly, cfapi::NoRecurse);
        QCOMPARE(*vfs->availability("local"), VfsItemAvailability::AllHydrated);
        setPinState(fakeFolder.localPath() + "online/sub", PinState::Unspecified, cfapi::NoRecurse);
        QCOMPARE(*vfs->availability("online"), VfsItemAvailability::AllDehydrated);

        triggerDownload(fakeFolder, "unspec/file1");
        setPinState(fakeFolder.localPath() + "local/file2", PinState::OnlineOnly, cfapi::NoRecurse);
        setPinState(fakeFolder.localPath() + "online/file2", PinState::AlwaysLocal, cfapi::NoRecurse);
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(*vfs->availability("unspec"), VfsItemAvailability::AllHydrated);
        QCOMPARE(*vfs->availability("local"), VfsItemAvailability::Mixed);
        QCOMPARE(*vfs->availability("online"), VfsItemAvailability::Mixed);

        vfs->setPinState("local", PinState::AlwaysLocal);
        vfs->setPinState("online", PinState::OnlineOnly);
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(*vfs->availability("online"), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability("local"), VfsItemAvailability::AlwaysLocal);

        auto r = vfs->availability("nonexistant");
        QVERIFY(!r);
        QCOMPARE(r.error(), Vfs::AvailabilityError::NoSuchItem);
    }

    void testPinStateLocals()
    {
        FakeFolder fakeFolder{ FileInfo() };
        auto vfs = setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("online");
        fakeFolder.remoteModifier().mkdir("unspec");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPinState(fakeFolder.localPath() + "local", PinState::AlwaysLocal, cfapi::NoRecurse);
        setPinState(fakeFolder.localPath() + "online", PinState::OnlineOnly, cfapi::NoRecurse);
        setPinState(fakeFolder.localPath() + "unspec", PinState::Unspecified, cfapi::NoRecurse);

        fakeFolder.localModifier().insert("file1");
        fakeFolder.localModifier().insert("online/file1");
        fakeFolder.localModifier().insert("online/file2");
        fakeFolder.localModifier().insert("local/file1");
        fakeFolder.localModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // root is unspecified
        QCOMPARE(*vfs->pinState("file1"), PinState::Unspecified);
        QCOMPARE(*vfs->pinState("local/file1"), PinState::AlwaysLocal);
        QCOMPARE(*vfs->pinState("online/file1"), PinState::Unspecified);
        QCOMPARE(*vfs->pinState("unspec/file1"), PinState::Unspecified);

        // Sync again: bad pin states of new local files usually take effect on second sync
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // When a file in an online-only folder is renamed, it retains its pin
        fakeFolder.localModifier().rename("online/file1", "online/file1rename");
        fakeFolder.remoteModifier().rename("online/file2", "online/file2rename");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(*vfs->pinState("online/file1rename"), PinState::Unspecified);
        QCOMPARE(*vfs->pinState("online/file2rename"), PinState::Unspecified);

        // When a folder is renamed, the pin states inside should be retained
        fakeFolder.localModifier().rename("online", "onlinerenamed1");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(*vfs->pinState("onlinerenamed1"), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState("onlinerenamed1/file1rename"), PinState::Unspecified);

        fakeFolder.remoteModifier().rename("onlinerenamed1", "onlinerenamed2");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(*vfs->pinState("onlinerenamed2"), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::Unspecified);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // When a file is deleted and later a new file has the same name, the old pin
        // state isn't preserved.
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::Unspecified);
        fakeFolder.remoteModifier().remove("onlinerenamed2/file1rename");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!vfs->pinState("onlinerenamed2/file1rename"));
        fakeFolder.remoteModifier().insert("onlinerenamed2/file1rename");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::OnlineOnly);

        // When a file is hydrated or dehydrated due to pin state it retains its pin state
        vfs->setPinState("onlinerenamed2/file1rename", PinState::AlwaysLocal);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("onlinerenamed2/file1rename"));
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::AlwaysLocal);

        vfs->setPinState("onlinerenamed2", PinState::Unspecified);
        vfs->setPinState("onlinerenamed2/file1rename", PinState::OnlineOnly);
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "onlinerenamed2/file1rename"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "onlinerenamed2/file1rename").exists());
        QCOMPARE(dbRecord(fakeFolder, "onlinerenamed2/file1rename")._type, ItemTypeVirtualFile);

        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::OnlineOnly);
    }

    void testIncompatiblePins()
    {
        FakeFolder fakeFolder{ FileInfo() };
        auto vfs = setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("online");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPinState(fakeFolder.localPath() + "local", PinState::AlwaysLocal, cfapi::NoRecurse);
        setPinState(fakeFolder.localPath() + "online", PinState::OnlineOnly, cfapi::NoRecurse);

        fakeFolder.localModifier().insert("local/file1");
        fakeFolder.localModifier().insert("online/file1");
        QVERIFY(fakeFolder.syncOnce());

        markForDehydration(fakeFolder, "local/file1");
        triggerDownload(fakeFolder, "online/file1");

        // the sync sets the changed files pin states to unspecified
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "online/file1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file1").exists());
        QCOMPARE(dbRecord(fakeFolder, "online/file1")._type, ItemTypeFile);

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "local/file1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file1").exists());
        QCOMPARE(dbRecord(fakeFolder, "local/file1")._type, ItemTypeVirtualFile);

        QCOMPARE(*vfs->pinState("online/file1"), PinState::Unspecified);
        QCOMPARE(*vfs->pinState("local/file1"), PinState::Unspecified);

        // no change on another sync
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!cfapi::isSparseFile(fakeFolder.localPath() + "online/file1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "online/file1").exists());
        QCOMPARE(dbRecord(fakeFolder, "online/file1")._type, ItemTypeFile);

        QVERIFY(cfapi::isSparseFile(fakeFolder.localPath() + "local/file1"));
        QVERIFY(QFileInfo(fakeFolder.localPath() + "local/file1").exists());
        QCOMPARE(dbRecord(fakeFolder, "local/file1")._type, ItemTypeVirtualFile);
    }
};

QTEST_GUILESS_MAIN(TestSyncCfApi)
#include "testsynccfapi.moc"
