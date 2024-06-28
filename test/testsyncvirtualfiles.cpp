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

using namespace OCC;

namespace {

QStringList findCaseClashConflicts(const FileInfo &dir)
{
    QStringList conflicts;
    for (const auto &item : dir.children) {
        if (item.name.contains("(case clash from")) {
            conflicts.append(item.path());
        }
    }
    return conflicts;
}

bool expectConflict(FileInfo state, const QString path)
{
    PathComponents pathComponents(path);
    auto base = state.find(pathComponents.parentDirComponents());
    if (!base)
        return false;
    for (const auto &item : qAsConst(base->children)) {
        if (item.name.startsWith(pathComponents.fileName()) && item.name.contains("(case clash from")) {
            return true;
        }
    }
    return false;
}
}

#define DVSUFFIX APPLICATION_DOTVIRTUALFILE_SUFFIX

bool itemInstruction(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
    return item->_instruction == instr;
}

SyncJournalFileRecord dbRecord(FakeFolder &folder, const QString &path)
{
    SyncJournalFileRecord record;
    [[maybe_unused]] const auto result = folder.syncJournal().getFileRecord(path, &record);
    return record;
}

void triggerDownload(FakeFolder &folder, const QByteArray &path)
{
    auto &journal = folder.syncJournal();
    SyncJournalFileRecord record;
    if (!journal.getFileRecord(path + DVSUFFIX, &record) || !record.isValid()) {
        return;
    }
    record._type = ItemTypeVirtualFileDownload;
    QVERIFY(journal.setFileRecord(record));
    journal.schedulePathForRemoteDiscovery(record._path);
}

void markForDehydration(FakeFolder &folder, const QByteArray &path)
{
    auto &journal = folder.syncJournal();
    SyncJournalFileRecord record;
    if (!journal.getFileRecord(path, &record) || !record.isValid()) {
        return;
    }
    record._type = ItemTypeVirtualFileDehydration;
    QVERIFY(journal.setFileRecord(record));
    journal.schedulePathForRemoteDiscovery(record._path);
}

QSharedPointer<Vfs> setupVfs(FakeFolder &folder)
{
    auto suffixVfs = QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::WithSuffix).release());
    folder.switchToVfs(suffixVfs);

    // Using this directly doesn't recursively unpin everything and instead leaves
    // the files in the hydration that that they start with
    folder.syncJournal().internalPinStates().setForPath("", PinState::Unspecified);

    return suffixVfs;
}

class TestSyncVirtualFiles : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

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
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1" DVSUFFIX).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFile);
        cleanup();

        // Another sync doesn't actually lead to changes
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1" DVSUFFIX).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Not even when the remote is rediscovered
        fakeFolder.syncJournal().forceRemoteDiscoveryNextSync();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1" DVSUFFIX).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Neither does a remote change
        fakeFolder.remoteModifier().appendByte("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_UPDATE_METADATA));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._fileSize, 65);
        cleanup();

        // If the local virtual file is removed, it should be gone remotely too
        if (!doLocalDiscovery)
            fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, { "A" });
        fakeFolder.localModifier().remove("A/a1" DVSUFFIX);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(!fakeFolder.remoteModifier().find("A/a1"));
        cleanup();

        // Restore the state prior to next test
        // Essentially repeating creation of virtual file
        fakeFolder.remoteModifier().insert("A/a1", 64);
        fakeFolder.remoteModifier().setModTime("A/a1", someDate);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1" DVSUFFIX).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFile);
        cleanup();

        // Remote rename is propagated
        fakeFolder.remoteModifier().rename("A/a1", "A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1m"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1m" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(
            itemInstruction(completeSpy, "A/a1m" DVSUFFIX, CSYNC_INSTRUCTION_RENAME)
            || (itemInstruction(completeSpy, "A/a1m" DVSUFFIX, CSYNC_INSTRUCTION_NEW)
                && itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE)));
        QCOMPARE(dbRecord(fakeFolder, "A/a1m" DVSUFFIX)._type, ItemTypeVirtualFile);
        cleanup();

        // Remote remove is propagated
        fakeFolder.remoteModifier().remove("A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1m" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(itemInstruction(completeSpy, "A/a1m" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a1" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a1m" DVSUFFIX).isValid());
        cleanup();

        // Edge case: Local virtual file but no db entry for some reason
        fakeFolder.remoteModifier().insert("A/a2", 64);
        fakeFolder.remoteModifier().insert("A/a3", 64);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3" DVSUFFIX));
        cleanup();

        QVERIFY(fakeFolder.syncEngine().journal()->deleteFileRecord("A/a2" DVSUFFIX));
        QVERIFY(fakeFolder.syncEngine().journal()->deleteFileRecord("A/a3" DVSUFFIX));
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(itemInstruction(completeSpy, "A/a2" DVSUFFIX, CSYNC_INSTRUCTION_UPDATE_METADATA));
        QVERIFY(dbRecord(fakeFolder, "A/a2" DVSUFFIX).isValid());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a3" DVSUFFIX));
        QVERIFY(itemInstruction(completeSpy, "A/a3" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a3" DVSUFFIX).isValid());
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
        fakeFolder.remoteModifier().insert("A/a1", 64);
        fakeFolder.remoteModifier().insert("A/a2", 64);
        fakeFolder.remoteModifier().mkdir("B");
        fakeFolder.remoteModifier().insert("B/b1", 64);
        fakeFolder.remoteModifier().insert("B/b2", 64);
        fakeFolder.remoteModifier().mkdir("C");
        fakeFolder.remoteModifier().insert("C/c1", 64);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/b2" DVSUFFIX));
        cleanup();

        // A: the correct file and a conflicting file are added, virtual files stay
        // B: same setup, but the virtual files are deleted by the user
        // C: user adds a *directory* locally
        fakeFolder.localModifier().insert("A/a1", 64);
        fakeFolder.localModifier().insert("A/a2", 30);
        fakeFolder.localModifier().insert("B/b1", 64);
        fakeFolder.localModifier().insert("B/b2", 30);
        fakeFolder.localModifier().remove("B/b1" DVSUFFIX);
        fakeFolder.localModifier().remove("B/b2" DVSUFFIX);
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
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("B/b2" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("C/c1" DVSUFFIX));

        // conflict files should exist
        QCOMPARE(fakeFolder.syncJournal().conflictRecordPaths().size(), 3);

        // nothing should have the virtual file tag
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b1")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "B/b2")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "C/c1")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a2" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "B/b1" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "B/b2" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "C/c1" DVSUFFIX).isValid());

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
        fakeFolder.remoteModifier().insert("A/new");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/new"));
        QVERIFY(fakeFolder.currentLocalState().find("A/new" DVSUFFIX));
        QVERIFY(fakeFolder.currentRemoteState().find("A/new"));
        QVERIFY(itemInstruction(completeSpy, "A/new" DVSUFFIX, CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/new" DVSUFFIX)._type, ItemTypeVirtualFile);
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
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a4" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a5" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a6" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a7" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/b1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/b2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/b3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/b4" DVSUFFIX));
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, "A/a1");
        triggerDownload(fakeFolder, "A/a2");
        triggerDownload(fakeFolder, "A/a3");
        triggerDownload(fakeFolder, "A/a4");
        triggerDownload(fakeFolder, "A/a5");
        triggerDownload(fakeFolder, "A/a6");
        triggerDownload(fakeFolder, "A/a7");
        // Download by renaming locally
        fakeFolder.localModifier().rename("A/b1" DVSUFFIX, "A/b1");
        fakeFolder.localModifier().rename("A/b2" DVSUFFIX, "A/b2");
        fakeFolder.localModifier().rename("A/b3" DVSUFFIX, "A/b3");
        fakeFolder.localModifier().rename("A/b4" DVSUFFIX, "A/b4");
        // Remote complications
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.remoteModifier().rename("A/a4", "A/a4m");
        fakeFolder.remoteModifier().appendByte("A/b2");
        fakeFolder.remoteModifier().remove("A/b3");
        fakeFolder.remoteModifier().rename("A/b4", "A/b4m");
        // Local complications
        fakeFolder.localModifier().insert("A/a5");
        fakeFolder.localModifier().insert("A/a6");
        fakeFolder.localModifier().remove("A/a6" DVSUFFIX);
        fakeFolder.localModifier().rename("A/a7" DVSUFFIX, "A/a7");

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a1")->_type, ItemTypeVirtualFileDownload);
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a2")->_type, ItemTypeVirtualFileDownload);
        QVERIFY(itemInstruction(completeSpy, "A/a2" DVSUFFIX, CSYNC_INSTRUCTION_NONE));
        QVERIFY(itemInstruction(completeSpy, "A/a3" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a4m", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a4" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a5", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a5" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a6", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/a7", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/b1", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/b2", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/b3", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/b4m" DVSUFFIX, CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/b4", CSYNC_INSTRUCTION_REMOVE));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1" DVSUFFIX).isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/a2")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a3").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/a4m")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a5")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a6")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/a7")._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, "A/b1")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/b1" DVSUFFIX).isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/b2")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/b3").isValid());
        QCOMPARE(dbRecord(fakeFolder, "A/b4m" DVSUFFIX)._type, ItemTypeVirtualFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a2" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a3" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a4" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a5" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a6" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a7" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/b1" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/b2" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/b3" DVSUFFIX).isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/b4" DVSUFFIX).isValid());

        triggerDownload(fakeFolder, "A/b4m");
        QVERIFY(fakeFolder.syncOnce());

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
            QVERIFY(fakeFolder.syncJournal().wipeErrorBlacklist() != -1);
        };
        cleanup();

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, "A/a1");
        fakeFolder.serverErrorPaths().append("A/a1", 500);
        QVERIFY(!fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_NONE));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFileDownload);
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        cleanup();

        fakeFolder.serverErrorPaths().clear();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_NONE));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1" DVSUFFIX).isValid());
    }

    void testNewFilesNotVirtual()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));

        fakeFolder.syncJournal().internalPinStates().setForPath("", PinState::AlwaysLocal);

        // Create a new remote file, it'll not be virtual
        fakeFolder.remoteModifier().insert("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
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
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/a4" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a5" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub2/a6" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/b1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/Sub/b2" DVSUFFIX));
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
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a3" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a4" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a5" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub2/a6" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/b1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/Sub/b2" DVSUFFIX));
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
        QVERIFY(fakeFolder.currentLocalState().find("A/Sub/SubSub/a7" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a7"));

        // Now download all files in "A"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a3" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/a4" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a5" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub2/a6" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/Sub/SubSub/a7" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/b1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("B/Sub/b2" DVSUFFIX));
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
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // If a file is renamed to <name>.owncloud, it becomes virtual
        fakeFolder.localModifier().rename("A/a1", "A/a1" DVSUFFIX);
        // If a file is renamed to <random>.owncloud, the rename propagates but the
        // file isn't made virtual the first sync run.
        fakeFolder.localModifier().rename("A/a2", "A/rand" DVSUFFIX);
        // dangling virtual files are removed
        fakeFolder.localModifier().insert("A/dangling" DVSUFFIX, 1, ' ');
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX)->size <= 1);
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(dbRecord(fakeFolder, "A/a1" DVSUFFIX)._type, ItemTypeVirtualFile);
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());

        QVERIFY(!fakeFolder.currentLocalState().find("A/a2"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/rand"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a2"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/rand"));
        QVERIFY(itemInstruction(completeSpy, "A/rand", CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "A/rand")._type == ItemTypeFile);

        QVERIFY(!fakeFolder.currentLocalState().find("A/dangling" DVSUFFIX));
        cleanup();
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

        QVERIFY(fakeFolder.currentLocalState().find("file1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("file2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("file3" DVSUFFIX));
        cleanup();

        fakeFolder.localModifier().rename("file1" DVSUFFIX, "renamed1" DVSUFFIX);
        fakeFolder.localModifier().rename("file2" DVSUFFIX, "renamed2" DVSUFFIX);
        triggerDownload(fakeFolder, "file2");
        triggerDownload(fakeFolder, "file3");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.currentLocalState().find("file1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("renamed1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("file1"));
        QVERIFY(fakeFolder.currentRemoteState().find("renamed1"));
        QVERIFY(itemInstruction(completeSpy, "renamed1" DVSUFFIX, CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "renamed1" DVSUFFIX).isValid());

        // file2 has a conflict between the download request and the rename:
        // the rename wins, the download is ignored
        QVERIFY(!fakeFolder.currentLocalState().find("file2"));
        QVERIFY(!fakeFolder.currentLocalState().find("file2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("renamed2" DVSUFFIX));
        QVERIFY(fakeFolder.currentRemoteState().find("renamed2"));
        QVERIFY(itemInstruction(completeSpy, "renamed2" DVSUFFIX, CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "renamed2" DVSUFFIX)._type == ItemTypeVirtualFile);

        QVERIFY(itemInstruction(completeSpy, "file3", CSYNC_INSTRUCTION_SYNC));
        QVERIFY(dbRecord(fakeFolder, "file3")._type == ItemTypeFile);
        cleanup();

        // Test rename while adding/removing vfs suffix
        fakeFolder.localModifier().rename("renamed1" DVSUFFIX, "R1");
        // Contents of file2 could also change at the same time...
        fakeFolder.localModifier().rename("file3", "R3" DVSUFFIX);
        QVERIFY(fakeFolder.syncOnce());
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
        fakeFolder.remoteModifier().insert("case5", 256, 'C');
        fakeFolder.remoteModifier().insert("case6", 256, 'C');
        QVERIFY(fakeFolder.syncOnce());

        triggerDownload(fakeFolder, "case4");
        triggerDownload(fakeFolder, "case6");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("case3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("case4"));
        QVERIFY(fakeFolder.currentLocalState().find("case5" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("case6"));
        cleanup();

        // Case 1: foo -> bar (tested elsewhere)
        // Case 2: foo.oc -> bar.oc (tested elsewhere)

        // Case 3: foo.oc -> bar (db unchanged)
        fakeFolder.localModifier().rename("case3" DVSUFFIX, "case3-rename");

        // Case 4: foo -> bar.oc (db unchanged)
        fakeFolder.localModifier().rename("case4", "case4-rename" DVSUFFIX);

        // Case 5: foo.oc -> bar.oc (db hydrate)
        fakeFolder.localModifier().rename("case5" DVSUFFIX, "case5-rename" DVSUFFIX);
        triggerDownload(fakeFolder, "case5");

        // Case 6: foo -> bar (db dehydrate)
        fakeFolder.localModifier().rename("case6", "case6-rename");
        markForDehydration(fakeFolder, "case6");

        QVERIFY(fakeFolder.syncOnce());

        // Case 3: the rename went though, hydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find("case3"));
        QVERIFY(!fakeFolder.currentLocalState().find("case3" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("case3-rename"));
        QVERIFY(fakeFolder.currentLocalState().find("case3-rename" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("case3"));
        QVERIFY(fakeFolder.currentRemoteState().find("case3-rename"));
        QVERIFY(itemInstruction(completeSpy, "case3-rename" DVSUFFIX, CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "case3-rename" DVSUFFIX)._type == ItemTypeVirtualFile);

        // Case 4: the rename went though, dehydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find("case4"));
        QVERIFY(!fakeFolder.currentLocalState().find("case4" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("case4-rename"));
        QVERIFY(!fakeFolder.currentLocalState().find("case4-rename" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("case4"));
        QVERIFY(fakeFolder.currentRemoteState().find("case4-rename"));
        QVERIFY(itemInstruction(completeSpy, "case4-rename", CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "case4-rename")._type == ItemTypeFile);

        // Case 5: the rename went though, hydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find("case5"));
        QVERIFY(!fakeFolder.currentLocalState().find("case5" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("case5-rename"));
        QVERIFY(fakeFolder.currentLocalState().find("case5-rename" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("case5"));
        QVERIFY(fakeFolder.currentRemoteState().find("case5-rename"));
        QVERIFY(itemInstruction(completeSpy, "case5-rename" DVSUFFIX, CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "case5-rename" DVSUFFIX)._type == ItemTypeVirtualFile);

        // Case 6: the rename went though, dehydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find("case6"));
        QVERIFY(!fakeFolder.currentLocalState().find("case6" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("case6-rename"));
        QVERIFY(!fakeFolder.currentLocalState().find("case6-rename" DVSUFFIX));
        QVERIFY(!fakeFolder.currentRemoteState().find("case6"));
        QVERIFY(fakeFolder.currentRemoteState().find("case6-rename"));
        QVERIFY(itemInstruction(completeSpy, "case6-rename", CSYNC_INSTRUCTION_RENAME));
        QVERIFY(dbRecord(fakeFolder, "case6-rename")._type == ItemTypeFile);
    }

    void testCreateFileWithTrailingSpaces_acceptAndRejectInvalidFileName()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const QString fileWithSpaces1(" foo");
        const QString fileWithSpaces2(" bar ");
        const QString fileWithSpaces3("bla ");
        const QString fileWithSpaces4("A/ foo");
        const QString fileWithSpaces5("A/ bar ");
        const QString fileWithSpaces6("A/bla ");

        fakeFolder.localModifier().insert(fileWithSpaces1);
        fakeFolder.localModifier().insert(fileWithSpaces2);
        fakeFolder.localModifier().insert(fileWithSpaces3);
        fakeFolder.localModifier().mkdir("A");
        fakeFolder.localModifier().insert(fileWithSpaces4);
        fakeFolder.localModifier().insert(fileWithSpaces5);
        fakeFolder.localModifier().insert(fileWithSpaces6);
        
        ItemCompletedSpy completeSpy(fakeFolder);
        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithSpaces1)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces2)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces3)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::FileNameInvalid);

        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces1);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces2);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces3);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces4);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces5);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces6);

        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithSpaces1)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces2)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces3)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::Success);
    }

    void testCreateFileWithTrailingSpaces_remoteDontGetRenamedAutomatically()
    {
        // On Windows we can't create files/folders with leading/trailing spaces locally. So, we have to fail those items. On other OSs - we just sync them down normally.
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces4("A/ foo");
        const QString fileWithSpaces5("A/ bar ");
        const QString fileWithSpaces6("A/bla ");

        const QString fileWithSpacesVirtual4(fileWithSpaces4 + DVSUFFIX);
        const QString fileWithSpacesVirtual5(fileWithSpaces5 + DVSUFFIX);
        const QString fileWithSpacesVirtual6(fileWithSpaces6 + DVSUFFIX);

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert(fileWithSpaces4);
        fakeFolder.remoteModifier().insert(fileWithSpaces5);
        fakeFolder.remoteModifier().insert(fileWithSpaces6);

        ItemCompletedSpy completeSpy(fakeFolder);
        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        if (Utility::isWindows()) {
            QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::FileNameInvalid);
            QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::FileNameInvalid);
            QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::FileNameInvalid);
        } else {
            QCOMPARE(completeSpy.findItem(fileWithSpacesVirtual4)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpacesVirtual5)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpacesVirtual6)->_status, SyncFileItem::Status::Success);
        }

    }

    void testCreateFileWithTrailingSpaces_remoteGetRenamedManually()
    {
        // On Windows we can't create files/folders with leading/trailing spaces locally. So, we have to fail those items. On other OSs - we just sync them down normally.
        FakeFolder fakeFolder{FileInfo()};
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces4("A/ foo");
        const QString fileWithSpaces5("A/ bar ");
        const QString fileWithSpaces6("A/bla ");

        const QString fileWithSpacesVirtual4(fileWithSpaces4 + DVSUFFIX);
        const QString fileWithSpacesVirtual5(fileWithSpaces5 + DVSUFFIX);
        const QString fileWithSpacesVirtual6(fileWithSpaces6 + DVSUFFIX);

        const QString fileWithoutSpaces4("A/foo");
        const QString fileWithoutSpaces5("A/bar");
        const QString fileWithoutSpaces6("A/bla");

        const QString fileWithoutSpacesVirtual4(fileWithoutSpaces4 + DVSUFFIX);
        const QString fileWithoutSpacesVirtual5(fileWithoutSpaces5 + DVSUFFIX);
        const QString fileWithoutSpacesVirtual6(fileWithoutSpaces6 + DVSUFFIX);

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert(fileWithSpaces4);
        fakeFolder.remoteModifier().insert(fileWithSpaces5);
        fakeFolder.remoteModifier().insert(fileWithSpaces6);

        ItemCompletedSpy completeSpy(fakeFolder);
        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        if (Utility::isWindows()) {
            QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::FileNameInvalid);
            QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::FileNameInvalid);
            QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::FileNameInvalid);
        } else {
            QCOMPARE(completeSpy.findItem(fileWithSpacesVirtual4)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpacesVirtual5)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpacesVirtual6)->_status, SyncFileItem::Status::Success);
        }
        
        fakeFolder.remoteModifier().rename(fileWithSpaces4, fileWithoutSpaces4);
        fakeFolder.remoteModifier().rename(fileWithSpaces5, fileWithoutSpaces5);
        fakeFolder.remoteModifier().rename(fileWithSpaces6, fileWithoutSpaces6);

        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        if (Utility::isWindows()) {
            QCOMPARE(completeSpy.findItem(fileWithoutSpaces4 + DVSUFFIX)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithoutSpaces5 + DVSUFFIX)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithoutSpaces6 + DVSUFFIX)->_status, SyncFileItem::Status::Success);
        } else {
            QCOMPARE(completeSpy.findItem(fileWithoutSpacesVirtual4)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithoutSpacesVirtual5)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithoutSpacesVirtual6)->_status, SyncFileItem::Status::Success);
        }
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
            QString placeholder = path + DVSUFFIX;
            return !fakeFolder.currentLocalState().find(path)
                && fakeFolder.currentLocalState().find(placeholder);
        };
        auto hasDehydratedDbEntries = [&](const QString &path) {
            SyncJournalFileRecord normal, suffix;
            return (!fakeFolder.syncJournal().getFileRecord(path, &normal) || !normal.isValid())
                && fakeFolder.syncJournal().getFileRecord(path + DVSUFFIX, &suffix) && suffix.isValid()
                && suffix._type == ItemTypeVirtualFile;
        };

        QVERIFY(isDehydrated("A/a1"));
        QVERIFY(hasDehydratedDbEntries("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a1" DVSUFFIX)->_type, ItemTypeVirtualFileDehydration);
        QCOMPARE(completeSpy.findItem("A/a1" DVSUFFIX)->_file, QStringLiteral("A/a1"));
        QCOMPARE(completeSpy.findItem("A/a1" DVSUFFIX)->_renameTarget, QStringLiteral("A/a1" DVSUFFIX));
        QVERIFY(isDehydrated("A/a2"));
        QVERIFY(hasDehydratedDbEntries("A/a2"));
        QVERIFY(itemInstruction(completeSpy, "A/a2" DVSUFFIX, CSYNC_INSTRUCTION_SYNC));
        QCOMPARE(completeSpy.findItem("A/a2" DVSUFFIX)->_type, ItemTypeVirtualFileDehydration);

        QVERIFY(!fakeFolder.currentLocalState().find("B/b1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(itemInstruction(completeSpy, "B/b1", CSYNC_INSTRUCTION_REMOVE));

        QVERIFY(!fakeFolder.currentLocalState().find("B/b2"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b2"));
        QVERIFY(isDehydrated("B/b3"));
        QVERIFY(hasDehydratedDbEntries("B/b3"));
        QVERIFY(itemInstruction(completeSpy, "B/b2", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "B/b3" DVSUFFIX, CSYNC_INSTRUCTION_NEW));

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

        QVERIFY(fakeFolder.currentLocalState().find("f1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/B/b1" DVSUFFIX));

        // Make local changes to a3
        fakeFolder.localModifier().remove("A/a3" DVSUFFIX);
        fakeFolder.localModifier().insert("A/a3" DVSUFFIX, 100);

        // Now wipe the virtuals

        SyncEngine::wipeVirtualFiles(fakeFolder.localPath(), fakeFolder.syncJournal(), *fakeFolder.syncEngine().syncOptions()._vfs);

        QVERIFY(!fakeFolder.currentLocalState().find("f1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/B/b1" DVSUFFIX));

        fakeFolder.switchToVfs(QSharedPointer<Vfs>(new VfsOff));
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("A/a3" DVSUFFIX)); // regular upload
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

        // Test 1: root is Unspecified
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("online/file1");
        fakeFolder.remoteModifier().insert("local/file1");
        fakeFolder.remoteModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("file1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("online/file1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file1" DVSUFFIX));

        // Test 2: change root to AlwaysLocal
        setPin("", PinState::AlwaysLocal);

        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("online/file2");
        fakeFolder.remoteModifier().insert("local/file2");
        fakeFolder.remoteModifier().insert("unspec/file2");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("file2"));
        QVERIFY(fakeFolder.currentLocalState().find("online/file2" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("local/file2"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file2" DVSUFFIX));

        // root file1 was hydrated due to its new pin state
        QVERIFY(fakeFolder.currentLocalState().find("file1"));

        // file1 is unchanged in the explicitly pinned subfolders
        QVERIFY(fakeFolder.currentLocalState().find("online/file1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file1" DVSUFFIX));

        // Test 3: change root to OnlineOnly
        setPin("", PinState::OnlineOnly);

        fakeFolder.remoteModifier().insert("file3");
        fakeFolder.remoteModifier().insert("online/file3");
        fakeFolder.remoteModifier().insert("local/file3");
        fakeFolder.remoteModifier().insert("unspec/file3");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("file3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("online/file3" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("local/file3"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file3" DVSUFFIX));

        // root file1 was dehydrated due to its new pin state
        QVERIFY(fakeFolder.currentLocalState().find("file1" DVSUFFIX));

        // file1 is unchanged in the explicitly pinned subfolders
        QVERIFY(fakeFolder.currentLocalState().find("online/file1" DVSUFFIX));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("unspec/file1" DVSUFFIX));
    }

    // Check what happens if vfs-suffixed files exist on the server or locally
    // while the file is hydrated
    void testSuffixFilesWhileLocalHydrated()
    {
        FakeFolder fakeFolder{ FileInfo() };

        ItemCompletedSpy completeSpy(fakeFolder);
        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // suffixed files are happily synced with Vfs::Off
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/test1" DVSUFFIX, 10, 'A');
        fakeFolder.remoteModifier().insert("A/test2" DVSUFFIX, 20, 'A');
        fakeFolder.remoteModifier().insert("A/file1" DVSUFFIX, 30, 'A');
        fakeFolder.remoteModifier().insert("A/file2", 40, 'A');
        fakeFolder.remoteModifier().insert("A/file2" DVSUFFIX, 50, 'A');
        fakeFolder.remoteModifier().insert("A/file3", 60, 'A');
        fakeFolder.remoteModifier().insert("A/file3" DVSUFFIX, 70, 'A');
        fakeFolder.remoteModifier().insert("A/file3" DVSUFFIX DVSUFFIX, 80, 'A');
        fakeFolder.remoteModifier().insert("A/remote1" DVSUFFIX, 30, 'A');
        fakeFolder.remoteModifier().insert("A/remote2", 40, 'A');
        fakeFolder.remoteModifier().insert("A/remote2" DVSUFFIX, 50, 'A');
        fakeFolder.remoteModifier().insert("A/remote3", 60, 'A');
        fakeFolder.remoteModifier().insert("A/remote3" DVSUFFIX, 70, 'A');
        fakeFolder.remoteModifier().insert("A/remote3" DVSUFFIX DVSUFFIX, 80, 'A');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        cleanup();

        // Enable suffix vfs
        setupVfs(fakeFolder);

        // A simple sync removes the files that are now ignored (?)
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file2" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // Add a real file where the suffixed file exists
        fakeFolder.localModifier().insert("A/test1", 11, 'A');
        fakeFolder.remoteModifier().insert("A/test2", 21, 'A');
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/test1", CSYNC_INSTRUCTION_NEW));
        // this isn't fully good since some code requires size == 1 for placeholders
        // (when renaming placeholder to real file). But the alternative would mean
        // special casing this to allow CONFLICT at virtual file creation level. Ew.
        QVERIFY(itemInstruction(completeSpy, "A/test2" DVSUFFIX, CSYNC_INSTRUCTION_UPDATE_METADATA));
        cleanup();

        // Local changes of suffixed file do nothing
        fakeFolder.localModifier().setContents("A/file1" DVSUFFIX, 'B');
        fakeFolder.localModifier().setContents("A/file2" DVSUFFIX, 'B');
        fakeFolder.localModifier().setContents("A/file3" DVSUFFIX, 'B');
        fakeFolder.localModifier().setContents("A/file3" DVSUFFIX DVSUFFIX, 'B');
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file2" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // Remote changes don't do anything either
        fakeFolder.remoteModifier().setContents("A/file1" DVSUFFIX, 'C');
        fakeFolder.remoteModifier().setContents("A/file2" DVSUFFIX, 'C');
        fakeFolder.remoteModifier().setContents("A/file3" DVSUFFIX, 'C');
        fakeFolder.remoteModifier().setContents("A/file3" DVSUFFIX DVSUFFIX, 'C');
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file2" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // Local removal: when not querying server
        fakeFolder.localModifier().remove("A/file1" DVSUFFIX);
        fakeFolder.localModifier().remove("A/file2" DVSUFFIX);
        fakeFolder.localModifier().remove("A/file3" DVSUFFIX);
        fakeFolder.localModifier().remove("A/file3" DVSUFFIX DVSUFFIX);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(completeSpy.findItem("A/file1" DVSUFFIX)->isEmpty());
        QVERIFY(completeSpy.findItem("A/file2" DVSUFFIX)->isEmpty());
        QVERIFY(completeSpy.findItem("A/file3" DVSUFFIX)->isEmpty());
        QVERIFY(completeSpy.findItem("A/file3" DVSUFFIX DVSUFFIX)->isEmpty());
        cleanup();

        // Local removal: when querying server
        fakeFolder.remoteModifier().setContents("A/file1" DVSUFFIX, 'D');
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file2" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // Remote removal
        fakeFolder.remoteModifier().remove("A/remote1" DVSUFFIX);
        fakeFolder.remoteModifier().remove("A/remote2" DVSUFFIX);
        fakeFolder.remoteModifier().remove("A/remote3" DVSUFFIX);
        fakeFolder.remoteModifier().remove("A/remote3" DVSUFFIX DVSUFFIX);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/remote1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/remote2" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/remote3" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/remote3" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // New files with a suffix aren't propagated downwards in the first place
        fakeFolder.remoteModifier().insert("A/new1" DVSUFFIX);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/new1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(fakeFolder.currentRemoteState().find("A/new1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/new1"));
        QVERIFY(!fakeFolder.currentLocalState().find("A/new1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/new1" DVSUFFIX DVSUFFIX));
        cleanup();
    }

    // Check what happens if vfs-suffixed files exist on the server or in the db
    void testExtraFilesLocalDehydrated()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);

        ItemCompletedSpy completeSpy(fakeFolder);
        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // create a bunch of local virtual files, in some instances
        // ignore remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/file1", 30, 'A');
        fakeFolder.remoteModifier().insert("A/file2", 40, 'A');
        fakeFolder.remoteModifier().insert("A/file3", 60, 'A');
        fakeFolder.remoteModifier().insert("A/file3" DVSUFFIX, 70, 'A');
        fakeFolder.remoteModifier().insert("A/file4", 80, 'A');
        fakeFolder.remoteModifier().insert("A/file4" DVSUFFIX, 90, 'A');
        fakeFolder.remoteModifier().insert("A/file4" DVSUFFIX DVSUFFIX, 100, 'A');
        fakeFolder.remoteModifier().insert("A/file5", 110, 'A');
        fakeFolder.remoteModifier().insert("A/file6", 120, 'A');
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/file1" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file2"));
        QVERIFY(fakeFolder.currentLocalState().find("A/file2" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file3"));
        QVERIFY(fakeFolder.currentLocalState().find("A/file3" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file4"));
        QVERIFY(fakeFolder.currentLocalState().find("A/file4" DVSUFFIX));
        QVERIFY(!fakeFolder.currentLocalState().find("A/file4" DVSUFFIX DVSUFFIX));
        QVERIFY(itemInstruction(completeSpy, "A/file1" DVSUFFIX, CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/file2" DVSUFFIX, CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/file3" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file4" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file4" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();

        // Create odd extra files locally and remotely
        fakeFolder.localModifier().insert("A/file1", 10, 'A');
        fakeFolder.localModifier().insert("A/file2" DVSUFFIX DVSUFFIX, 10, 'A');
        fakeFolder.remoteModifier().insert("A/file5" DVSUFFIX, 10, 'A');
        fakeFolder.localModifier().insert("A/file6", 10, 'A');
        fakeFolder.remoteModifier().insert("A/file6" DVSUFFIX, 10, 'A');
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/file1", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/file1" DVSUFFIX, CSYNC_INSTRUCTION_REMOVE)); // it's now a pointless real virtual file
        QVERIFY(itemInstruction(completeSpy, "A/file2" DVSUFFIX DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file5" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/file6", CSYNC_INSTRUCTION_CONFLICT));
        QVERIFY(itemInstruction(completeSpy, "A/file6" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        cleanup();
    }

    void testAvailability()
    {
        FakeFolder fakeFolder{ FileInfo() };
        auto vfs = setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        auto setPin = [&] (const QByteArray &path, PinState state) {
            fakeFolder.syncJournal().internalPinStates().setForPath(path, state);
        };

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("local/sub");
        fakeFolder.remoteModifier().mkdir("online");
        fakeFolder.remoteModifier().mkdir("online/sub");
        fakeFolder.remoteModifier().mkdir("unspec");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPin("local", PinState::AlwaysLocal);
        setPin("online", PinState::OnlineOnly);
        setPin("unspec", PinState::Unspecified);

        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("online/file1");
        fakeFolder.remoteModifier().insert("online/file2");
        fakeFolder.remoteModifier().insert("local/file1");
        fakeFolder.remoteModifier().insert("local/file2");
        fakeFolder.remoteModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());

        // root is unspecified
        QCOMPARE(*vfs->availability("file1" DVSUFFIX, Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AllDehydrated);
        QCOMPARE(*vfs->availability("local", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AlwaysLocal);
        QCOMPARE(*vfs->availability("local/file1", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AlwaysLocal);
        QCOMPARE(*vfs->availability("online", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability("online/file1" DVSUFFIX, Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability("unspec", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AllDehydrated);
        QCOMPARE(*vfs->availability("unspec/file1" DVSUFFIX, Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AllDehydrated);

        // Subitem pin states can ruin "pure" availabilities
        setPin("local/sub", PinState::OnlineOnly);
        QCOMPARE(*vfs->availability("local", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AllHydrated);
        setPin("online/sub", PinState::Unspecified);
        QCOMPARE(*vfs->availability("online", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AllDehydrated);

        triggerDownload(fakeFolder, "unspec/file1");
        setPin("local/file2", PinState::OnlineOnly);
        setPin("online/file2" DVSUFFIX, PinState::AlwaysLocal);
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(*vfs->availability("unspec", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AllHydrated);
        QCOMPARE(*vfs->availability("local", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::Mixed);
        QCOMPARE(*vfs->availability("online", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::Mixed);

        QVERIFY(vfs->setPinState("local", PinState::AlwaysLocal));
        QVERIFY(vfs->setPinState("online", PinState::OnlineOnly));
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(*vfs->availability("online", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability("local", Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::AlwaysLocal);

        auto r = vfs->availability("nonexistent", Vfs::AvailabilityRecursivity::RecursiveAvailability);
        QVERIFY(!r);
        QCOMPARE(r.error(), Vfs::AvailabilityError::NoSuchItem);
    }

    void testPinStateLocals()
    {
        FakeFolder fakeFolder{ FileInfo() };
        auto vfs = setupVfs(fakeFolder);
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

        fakeFolder.localModifier().insert("file1");
        fakeFolder.localModifier().insert("online/file1");
        fakeFolder.localModifier().insert("online/file2");
        fakeFolder.localModifier().insert("local/file1");
        fakeFolder.localModifier().insert("unspec/file1");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // root is unspecified
        QCOMPARE(*vfs->pinState("file1" DVSUFFIX), PinState::Unspecified);
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
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::OnlineOnly);
        fakeFolder.remoteModifier().insert("onlinerenamed2/file1rename");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename" DVSUFFIX), PinState::OnlineOnly);

        // When a file is hydrated or dehydrated due to pin state it retains its pin state
        QVERIFY(vfs->setPinState("onlinerenamed2/file1rename" DVSUFFIX, PinState::AlwaysLocal));
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("onlinerenamed2/file1rename"));
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename"), PinState::AlwaysLocal);

        QVERIFY(vfs->setPinState("onlinerenamed2", PinState::Unspecified));
        QVERIFY(vfs->setPinState("onlinerenamed2/file1rename", PinState::OnlineOnly));
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("onlinerenamed2/file1rename" DVSUFFIX));
        QCOMPARE(*vfs->pinState("onlinerenamed2/file1rename" DVSUFFIX), PinState::OnlineOnly);
    }

    void testIncompatiblePins()
    {
        FakeFolder fakeFolder{ FileInfo() };
        auto vfs = setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        auto setPin = [&] (const QByteArray &path, PinState state) {
            fakeFolder.syncJournal().internalPinStates().setForPath(path, state);
        };

        fakeFolder.remoteModifier().mkdir("local");
        fakeFolder.remoteModifier().mkdir("online");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPin("local", PinState::AlwaysLocal);
        setPin("online", PinState::OnlineOnly);

        fakeFolder.localModifier().insert("local/file1");
        fakeFolder.localModifier().insert("online/file1");
        QVERIFY(fakeFolder.syncOnce());

        markForDehydration(fakeFolder, "local/file1");
        triggerDownload(fakeFolder, "online/file1");

        // the sync sets the changed files pin states to unspecified
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("online/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1" DVSUFFIX));
        QCOMPARE(*vfs->pinState("online/file1"), PinState::Unspecified);
        QCOMPARE(*vfs->pinState("local/file1" DVSUFFIX), PinState::Unspecified);

        // no change on another sync
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("online/file1"));
        QVERIFY(fakeFolder.currentLocalState().find("local/file1" DVSUFFIX));
    }

    void testPlaceHolderExist() {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.remoteModifier().insert("A/a1" DVSUFFIX, 111);
        fakeFolder.remoteModifier().insert("A/hello" DVSUFFIX, 222);
        QVERIFY(fakeFolder.syncOnce());
        auto vfs = setupVfs(fakeFolder);

        ItemCompletedSpy completeSpy(fakeFolder);
        auto cleanup = [&]() { completeSpy.clear(); };
        cleanup();

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/hello" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));

        fakeFolder.remoteModifier().insert("A/a2" DVSUFFIX);
        fakeFolder.remoteModifier().insert("A/hello", 12);
        fakeFolder.localModifier().insert("A/igno" DVSUFFIX, 123);
        cleanup();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        QVERIFY(itemInstruction(completeSpy, "A/igno" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));

        // verify that the files are still present
        QCOMPARE(fakeFolder.currentLocalState().find("A/hello" DVSUFFIX)->size, 222);
        QCOMPARE(*fakeFolder.currentLocalState().find("A/hello" DVSUFFIX),
                 *fakeFolder.currentRemoteState().find("A/hello" DVSUFFIX));
        QCOMPARE(fakeFolder.currentLocalState().find("A/igno" DVSUFFIX)->size, 123);

        cleanup();
        // Dehydrate
        QVERIFY(vfs->setPinState(QString(), PinState::OnlineOnly));
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(itemInstruction(completeSpy, "A/igno" DVSUFFIX, CSYNC_INSTRUCTION_IGNORE));
        // verify that the files are still present
        QCOMPARE(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX)->size, 111);
        QCOMPARE(fakeFolder.currentLocalState().find("A/hello" DVSUFFIX)->size, 222);
        QCOMPARE(*fakeFolder.currentLocalState().find("A/hello" DVSUFFIX),
                 *fakeFolder.currentRemoteState().find("A/hello" DVSUFFIX));
        QCOMPARE(*fakeFolder.currentLocalState().find("A/a1"),
                 *fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(fakeFolder.currentLocalState().find("A/igno" DVSUFFIX)->size, 123);

        // Now disable vfs and check that all files are still there
        cleanup();
        SyncEngine::wipeVirtualFiles(fakeFolder.localPath(), fakeFolder.syncJournal(), *vfs);
        fakeFolder.switchToVfs(QSharedPointer<Vfs>(new VfsOff));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentLocalState().find("A/a1" DVSUFFIX)->size, 111);
        QCOMPARE(fakeFolder.currentLocalState().find("A/hello")->size, 12);
        QCOMPARE(fakeFolder.currentLocalState().find("A/hello" DVSUFFIX)->size, 222);
        QCOMPARE(fakeFolder.currentLocalState().find("A/igno" DVSUFFIX)->size, 123);
    }

    void testUpdateMetadataErrorManagement()
    {
        FakeFolder fakeFolder{FileInfo{}};
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Existing files are propagated just fine in both directions
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.remoteModifier().insert(QStringLiteral("aaa/subfolder/bar"));
        QVERIFY(fakeFolder.syncOnce());

        // New files on the remote create virtual files
        fakeFolder.remoteModifier().setModTime(QStringLiteral("aaa/subfolder/bar"), QDateTime::fromSecsSinceEpoch(0));
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.syncOnce());
    }

    void testInvalidFutureMtimeRecovery()
    {
        constexpr auto FUTURE_MTIME = 0xFFFFFFFF;
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{FileInfo{}};
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString fooFileAaaSubFolder("aaa/subfolder/foo");
        const QString barFileAaaSubFolder("aaa/subfolder/bar");

        fakeFolder.remoteModifier().insert(fooFileRootFolder);
        fakeFolder.remoteModifier().insert(barFileRootFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.remoteModifier().insert(fooFileSubFolder);
        fakeFolder.remoteModifier().insert(barFileSubFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.remoteModifier().insert(fooFileAaaSubFolder);
        fakeFolder.remoteModifier().insert(barFileAaaSubFolder);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().setModTimeKeepEtag(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(barFileRootFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(barFileSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.localModifier().setModTime(fooFileRootFolder + DVSUFFIX, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(barFileRootFolder + DVSUFFIX, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(fooFileSubFolder + DVSUFFIX, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(barFileSubFolder + DVSUFFIX, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(fooFileAaaSubFolder + DVSUFFIX, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(barFileAaaSubFolder + DVSUFFIX, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());
    }

    void testInvalidMtimeLocalDiscovery()
    {
        constexpr auto INVALID_MTIME1 = 0;
        constexpr auto INVALID_MTIME2 = 0xFFFFFFFF;
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{FileInfo{}};
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy statusSpy(&fakeFolder.syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged);

        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString fooFileAaaSubFolder("aaa/subfolder/foo");
        const QString barFileAaaSubFolder("aaa/subfolder/bar");

        auto checkStatus = [&]() -> SyncFileStatus::SyncFileStatusTag {
            auto file = QFileInfo{fakeFolder.syncEngine().localPath(), barFileAaaSubFolder};
            auto locPath = fakeFolder.syncEngine().localPath();
            auto itemFound = false;
            // Start from the end to get the latest status
            for (int i = statusSpy.size() - 1; i >= 0 && !itemFound; --i) {
                if (QFileInfo(statusSpy.at(i)[0].toString()) == file) {
                    itemFound = true;
                    return statusSpy.at(i)[1].value<SyncFileStatus>().tag();
                }
            }

            return {};
        };

        fakeFolder.localModifier().insert(fooFileRootFolder);
        fakeFolder.localModifier().insert(barFileRootFolder);
        fakeFolder.localModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.localModifier().insert(fooFileSubFolder);
        fakeFolder.localModifier().insert(barFileSubFolder);
        fakeFolder.localModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.localModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.localModifier().insert(fooFileAaaSubFolder);
        fakeFolder.localModifier().insert(barFileAaaSubFolder);
        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME1));

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();

        QCOMPARE(checkStatus(), SyncFileStatus::StatusError);

        fakeFolder.execUntilFinished();

        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.localModifier().appendByte(barFileAaaSubFolder);
        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME1));

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();

        QCOMPARE(checkStatus(), SyncFileStatus::StatusError);

        fakeFolder.execUntilFinished();

        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.localModifier().appendByte(barFileAaaSubFolder);
        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME2));

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();

        QCOMPARE(checkStatus(), SyncFileStatus::StatusError);

        fakeFolder.execUntilFinished();
    }

    void testServer_caseClash_createConflict()
    {
        constexpr auto testLowerCaseFile = "test";
        constexpr auto testUpperCaseFile = "TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{FileInfo{}};
        setupVfs(fakeFolder);

        fakeFolder.remoteModifier().insert("otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_subFolderCaseClash_createConflict()
    {
        constexpr auto testLowerCaseFile = "a/b/test";
        constexpr auto testUpperCaseFile = "a/b/TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder);

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().mkdir("a/b");
        fakeFolder.remoteModifier().insert("a/b/otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_caseClash_createConflictOnMove()
    {
        constexpr auto testLowerCaseFile = "test";
        constexpr auto testUpperCaseFile = "TEST2";
        constexpr auto testUpperCaseFileAfterMove = "TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder);

        fakeFolder.remoteModifier().insert("otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, false);

        fakeFolder.remoteModifier().rename(testUpperCaseFile, testUpperCaseFileAfterMove);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflictAfterMove = expectConflict(fakeFolder.currentLocalState(), testUpperCaseFileAfterMove);
        QCOMPARE(hasConflictAfterMove, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_subFolderCaseClash_createConflictOnMove()
    {
        constexpr auto testLowerCaseFile = "a/b/test";
        constexpr auto testUpperCaseFile = "a/b/TEST2";
        constexpr auto testUpperCaseFileAfterMove = "a/b/TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder);

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().mkdir("a/b");
        fakeFolder.remoteModifier().insert("a/b/otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, false);

        fakeFolder.remoteModifier().rename(testUpperCaseFile, testUpperCaseFileAfterMove);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflictAfterMove = expectConflict(fakeFolder.currentLocalState(), testUpperCaseFileAfterMove);
        QCOMPARE(hasConflictAfterMove, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testDataFingerPrint()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder);

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().mkdir("a/b");
        fakeFolder.remoteModifier().mkdir("a/b/d");
        fakeFolder.remoteModifier().insert("a/b/otherFile.txt");

        //Server support finger print, but none is set.
        fakeFolder.remoteModifier().extraDavProperties = "<oc:data-fingerprint></oc:data-fingerprint>";

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("a/b/otherFile.txt");
        fakeFolder.remoteModifier().remove("a/b/d");
        fakeFolder.remoteModifier().extraDavProperties = "<oc:data-fingerprint>initial_finger_print</oc:data-fingerprint>";

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLockFile_lockedFileReadOnly_afterSync()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder);

        ItemCompletedSpy completeSpy(fakeFolder);

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1") + DVSUFFIX)->_locked, OCC::SyncFileItem::LockStatus::UnlockedItem);
        OCC::SyncJournalFileRecord fileRecordBefore;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1") + DVSUFFIX, &fileRecordBefore));
        QVERIFY(fileRecordBefore.isValid());
        QVERIFY(!fileRecordBefore._lockstate._locked);

        const auto localFileNotLocked = QFileInfo{fakeFolder.localPath() + u"A/a1" + DVSUFFIX};
        QVERIFY(localFileNotLocked.isWritable());

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("A/a1"), FileModifier::LockState::FileLocked, 1, QStringLiteral("Nextcloud Office"), {}, QStringLiteral("richdocuments"), QDateTime::currentDateTime().toSecsSinceEpoch(), 1226);
        fakeFolder.remoteModifier().setModTimeKeepEtag(QStringLiteral("A/a1"), QDateTime::currentDateTime());
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1") + DVSUFFIX)->_locked, OCC::SyncFileItem::LockStatus::LockedItem);
        OCC::SyncJournalFileRecord fileRecordLocked;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1") + DVSUFFIX, &fileRecordLocked));
        QVERIFY(fileRecordLocked.isValid());
        QVERIFY(fileRecordLocked._lockstate._locked);

        const auto localFileLocked = QFileInfo{fakeFolder.localPath() + u"A/a1" + DVSUFFIX};
        QVERIFY(!localFileLocked.isWritable());
    }
};

QTEST_GUILESS_MAIN(TestSyncVirtualFiles)
#include "testsyncvirtualfiles.moc"
