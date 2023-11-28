/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "common/vfs.h"
#include "libsync/syncengine.h"
#include "libsync/theme.h"
#include "testutils/syncenginetestutils.h"
#include <QtTest>

using namespace OCC;

auto itemInstruction(const ItemCompletedSpy &spy, const QString &path)
{
    auto item = spy.findItem(path);
    Q_ASSERT(!item.isNull());
    return item->instruction();
}

SyncJournalFileRecord dbRecord(FakeFolder &folder, const QString &path)
{
    SyncJournalFileRecord record;
    folder.syncJournal().getFileRecord(path, &record);
    return record;
}

void triggerDownload(FakeFolder &folder, const QString &path)
{
    auto &journal = folder.syncJournal();
    SyncJournalFileRecord record;
    journal.getFileRecord(path + Theme::instance()->appDotVirtualFileSuffix(), &record);
    if (!record.isValid())
        return;
    record._type = ItemTypeVirtualFileDownload;
    journal.setFileRecord(record);
    journal.schedulePathForRemoteDiscovery(record._path);
}

// TODO: triggering dehydration by other means than the pin state is an unsupported scenario
void markForDehydration(FakeFolder &folder, const QString &path)
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

QSharedPointer<Vfs> setupVfs(FakeFolder &folder, Vfs::Mode vfsMode = Vfs::WithSuffix)
{
    auto suffixVfs = QSharedPointer<Vfs>(VfsPluginManager::instance().createVfsFromPlugin(vfsMode).release());
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
        fakeFolder.syncEngine().setPromtRemoveAllFiles(false);

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
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"), 64_b);
        auto someDate = QDateTime(QDate(1984, 07, 30), QTime(1,3,2));
        fakeFolder.remoteModifier().setModTime(QStringLiteral("A/a1"), someDate);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QString(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        cleanup();

        // Another sync doesn't actually lead to changes
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QString(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Not even when the remote is rediscovered
        fakeFolder.syncJournal().forceRemoteDiscoveryNextSync();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QString(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).lastModified(), someDate);
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        QVERIFY(completeSpy.findItem(QStringLiteral("A")));
        QVERIFY2(completeSpy.size() == 1, "Only the meta data of A was updated");
        cleanup();

        // Neither does a remote change
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_UPDATE_METADATA);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._fileSize, 65);
        cleanup();

        // If the local virtual file file is removed, it'll just be recreated
        if (!doLocalDiscovery)
            fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {QStringLiteral("A")});
        fakeFolder.localModifier().remove(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._fileSize, 65);
        cleanup();

        // Remote rename is propagated
        fakeFolder.remoteModifier().rename(QStringLiteral("A/a1"), QStringLiteral("A/a1m"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1m")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1m")));
        QVERIFY(itemInstruction(completeSpy, QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix()) == CSYNC_INSTRUCTION_RENAME
            || (itemInstruction(completeSpy, QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix()) == CSYNC_INSTRUCTION_NEW
                && itemInstruction(completeSpy, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()) == CSYNC_INSTRUCTION_REMOVE));
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        cleanup();

        // Remote remove is propagated
        fakeFolder.remoteModifier().remove(QStringLiteral("A/a1m"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/a1m")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_REMOVE);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1m") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        cleanup();

        // Edge case: Local virtual file but no db entry for some reason
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a2"), 64_b);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a3"), 64_b);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()));
        cleanup();

        fakeFolder.syncEngine().journal()->deleteFileRecord(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix());
        fakeFolder.syncEngine().journal()->deleteFileRecord(QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix());
        fakeFolder.remoteModifier().remove(QStringLiteral("A/a3"));
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_UPDATE_METADATA);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_REMOVE);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
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
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"), 64_b);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a2"), 64_b);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("B"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B/b1"), 64_b);
        fakeFolder.remoteModifier().insert(QStringLiteral("B/b2"), 64_b);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("C"));
        fakeFolder.remoteModifier().insert(QStringLiteral("C/c1"), 64_b);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/b2") + Theme::instance()->appDotVirtualFileSuffix()));
        cleanup();

        // A: the correct file and a conflicting file are added, virtual files stay
        // B: same setup, but the virtual files are deleted by the user
        // C: user adds a *directory* locally
        fakeFolder.localModifier().insert(QStringLiteral("A/a1"), 64_b);
        fakeFolder.localModifier().insert(QStringLiteral("A/a2"), 30_b);
        fakeFolder.localModifier().insert(QStringLiteral("B/b1"), 64_b);
        fakeFolder.localModifier().insert(QStringLiteral("B/b2"), 30_b);
        fakeFolder.localModifier().remove(QStringLiteral("B/b1") + Theme::instance()->appDotVirtualFileSuffix());
        fakeFolder.localModifier().remove(QStringLiteral("B/b2") + Theme::instance()->appDotVirtualFileSuffix());
        fakeFolder.localModifier().mkdir(QStringLiteral("C/c1"));
        fakeFolder.localModifier().insert(QStringLiteral("C/c1/foo"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        // Everything is CONFLICT since mtimes are different even for a1/b1
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a2")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("B/b1")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("B/b2")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("C/c1")), CSYNC_INSTRUCTION_CONFLICT);

        // no virtual file files should remain
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("C/c1") + Theme::instance()->appDotVirtualFileSuffix()));

        // conflict files should exist
        QCOMPARE(fakeFolder.syncJournal().conflictRecordPaths().size(), 3);

        // nothing should have the virtual file tag
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a2"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("B/b1"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("B/b2"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("C/c1"))._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("B/b1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("B/b2") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("C/c1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());

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
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        cleanup();

        // Existing files are propagated just fine in both directions
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.localModifier().insert(QStringLiteral("A/a3"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        cleanup();

        // New files on the remote create virtual files
        fakeFolder.remoteModifier().insert(QStringLiteral("A/new"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/new")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/new") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/new")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/new") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/new") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        cleanup();
    }

    void testVirtualFileDownload()
    {
        FakeFolder fakeFolder{ FileInfo() };
        fakeFolder.syncEngine().setPromtRemoveAllFiles(false);
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a4"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a5"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a6"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a7"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/b1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/b2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/b3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/b4"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a4") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a5") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a6") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a7") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/b1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/b2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/b3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/b4") + Theme::instance()->appDotVirtualFileSuffix()));
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, QStringLiteral("A/a1"));
        triggerDownload(fakeFolder, QStringLiteral("A/a2"));
        triggerDownload(fakeFolder, QStringLiteral("A/a3"));
        triggerDownload(fakeFolder, QStringLiteral("A/a4"));
        triggerDownload(fakeFolder, QStringLiteral("A/a5"));
        triggerDownload(fakeFolder, QStringLiteral("A/a6"));
        triggerDownload(fakeFolder, QStringLiteral("A/a7"));
        // Download by renaming locally
        fakeFolder.localModifier().rename(QStringLiteral("A/b1") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("A/b1"));
        fakeFolder.localModifier().rename(QStringLiteral("A/b2") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("A/b2"));
        fakeFolder.localModifier().rename(QStringLiteral("A/b3") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("A/b3"));
        fakeFolder.localModifier().rename(QStringLiteral("A/b4") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("A/b4"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        // Remote complications
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().remove(QStringLiteral("A/a3"));
        fakeFolder.remoteModifier().rename(QStringLiteral("A/a4"), QStringLiteral("A/a4m"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/b2"));
        fakeFolder.remoteModifier().remove(QStringLiteral("A/b3"));
        fakeFolder.remoteModifier().rename(QStringLiteral("A/b4"), QStringLiteral("A/b4m"));
        // Local complications
        fakeFolder.localModifier().insert(QStringLiteral("A/a5"));
        fakeFolder.localModifier().insert(QStringLiteral("A/a6"));
        fakeFolder.localModifier().remove(QStringLiteral("A/a6") + Theme::instance()->appDotVirtualFileSuffix());
        fakeFolder.localModifier().rename(QStringLiteral("A/a7") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("A/a7"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1")), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1"))->_type, ItemTypeVirtualFileDownload);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a2")), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a2"))->_type, ItemTypeVirtualFileDownload);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_REMOVE);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a4m")), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a4") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_REMOVE);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a5")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a5") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_REMOVE);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a6")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a7")), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/b1")), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/b2")), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/b3")), CSYNC_INSTRUCTION_REMOVE);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/b4m") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/b4")), CSYNC_INSTRUCTION_REMOVE);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1"))._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a2"))._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a3")).isValid());
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a4m"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a5"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a6"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a7"))._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/b1"))._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/b1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/b2"))._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/b3")).isValid());
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/b4m") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a3") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a4") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a5") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a6") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a7") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/b1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/b2") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/b3") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/b4") + Theme::instance()->appDotVirtualFileSuffix()).isValid());

        triggerDownload(fakeFolder, QStringLiteral("A/b4m"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

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
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        cleanup();

        // Download by changing the db entry
        triggerDownload(fakeFolder, QStringLiteral("A/a1"));
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/a1"), 500);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1")), CSYNC_INSTRUCTION_SYNC);
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFileDownload);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1")).isValid());
        cleanup();

        fakeFolder.serverErrorPaths().clear();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1")), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1"))._type, ItemTypeFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());
    }

    void testNewFilesNotVirtual()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));

        fakeFolder.syncJournal().internalPinStates().setForPath("", PinState::AlwaysLocal);

        // Create a new remote file, it'll not be virtual
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
    }

    void testDownloadRecursive()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A/Sub"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A/Sub/SubSub"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A/Sub2"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("B"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("B/Sub"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/Sub/a3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/Sub/a4"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/Sub/SubSub/a5"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/Sub2/a6"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B/b1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B/Sub/b2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a4") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a5") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub2/a6") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/b1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/Sub/b2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a3")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a4")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a5")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub2/a6")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/Sub/b2")));


        // Download All file in the directory A/Sub
        // (as in Folder::downloadVirtualFile)
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A/Sub");

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a4") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a5") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub2/a6") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/b1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/Sub/b2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a3")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a4")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a5")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub2/a6")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/Sub/b2")));

        // Add a file in a subfolder that was downloaded
        // Currently, this continue to add it as a virtual file.
        fakeFolder.remoteModifier().insert(QStringLiteral("A/Sub/SubSub/a7"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a7") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a7")));

        // Now download all files in "A"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("A");
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a4") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a5") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub2/a6") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a7") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/b1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("B/Sub/b2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a3")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/a4")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a5")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub2/a6")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/Sub/SubSub/a7")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/Sub/b2")));

        // Now download remaining files in "B"
        fakeFolder.syncJournal().markVirtualFileForDownloadRecursively("B");
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
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
        fakeFolder.localModifier().rename(QStringLiteral("A/a1"), QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix());
        // If a file is renamed to <random>.owncloud, the rename propagates but the
        // file isn't made virtual the first sync run.
        fakeFolder.localModifier().rename(QStringLiteral("A/a2"), QStringLiteral("A/rand") + Theme::instance()->appDotVirtualFileSuffix());
        // dangling virtual files are removed
        fakeFolder.localModifier().insert(QStringLiteral("A/dangling") + Theme::instance()->appDotVirtualFileSuffix(), 1_b, ' ');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())->contentSize <= 1);
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(dbRecord(fakeFolder, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())._type, ItemTypeVirtualFile);
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1")).isValid());

        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/rand")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("A/a2")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/rand")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/rand")), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("A/rand"))._type == ItemTypeFile);

        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/dangling") + Theme::instance()->appDotVirtualFileSuffix()));
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

        fakeFolder.remoteModifier().insert(QStringLiteral("file1"), 128_b, 'C');
        fakeFolder.remoteModifier().insert(QStringLiteral("file2"), 256_b, 'C');
        fakeFolder.remoteModifier().insert(QStringLiteral("file3"), 256_b, 'C');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file3") + Theme::instance()->appDotVirtualFileSuffix()));
        cleanup();

        fakeFolder.localModifier().rename(
            QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("renamed1") + Theme::instance()->appDotVirtualFileSuffix());
        fakeFolder.localModifier().rename(
            QStringLiteral("file2") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("renamed2") + Theme::instance()->appDotVirtualFileSuffix());
        triggerDownload(fakeFolder, QStringLiteral("file2"));
        triggerDownload(fakeFolder, QStringLiteral("file3"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("renamed1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("file1")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("renamed1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("renamed1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("renamed1") + Theme::instance()->appDotVirtualFileSuffix()).isValid());

        // file2 has a conflict between the download request and the rename:
        // the rename wins, the download is ignored
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("file2")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("file2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("renamed2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("renamed2")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("renamed2") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("renamed2") + Theme::instance()->appDotVirtualFileSuffix())._type == ItemTypeVirtualFile);

        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("file3")), CSYNC_INSTRUCTION_SYNC);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("file3"))._type == ItemTypeFile);
        cleanup();

        // Test rename while adding/removing vfs suffix
        fakeFolder.localModifier().rename(QStringLiteral("renamed1") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("R1"));
        // Contents of file2 could also change at the same time...
        fakeFolder.localModifier().rename(QStringLiteral("file3"), QStringLiteral("R3") + Theme::instance()->appDotVirtualFileSuffix());
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
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

        fakeFolder.remoteModifier().insert(QStringLiteral("case3"), 128_b, 'C');
        fakeFolder.remoteModifier().insert(QStringLiteral("case4"), 256_b, 'C');
        fakeFolder.remoteModifier().insert(QStringLiteral("case5"), 256_b, 'C');
        fakeFolder.remoteModifier().insert(QStringLiteral("case6"), 256_b, 'C');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        triggerDownload(fakeFolder, QStringLiteral("case4"));
        triggerDownload(fakeFolder, QStringLiteral("case6"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case4")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case5") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case6")));
        cleanup();

        // Case 1: foo -> bar (tested elsewhere)
        // Case 2: foo.oc -> bar.oc (tested elsewhere)

        // Case 3: foo.oc -> bar (db unchanged)
        fakeFolder.localModifier().rename(QStringLiteral("case3") + Theme::instance()->appDotVirtualFileSuffix(), QStringLiteral("case3-rename"));

        // Case 4: foo -> bar.oc (db unchanged)
        fakeFolder.localModifier().rename(QStringLiteral("case4"), QStringLiteral("case4-rename") + Theme::instance()->appDotVirtualFileSuffix());

        // Case 5: foo.oc -> bar.oc (db hydrate)
        fakeFolder.localModifier().rename(QStringLiteral("case5") + Theme::instance()->appDotVirtualFileSuffix(),
            QStringLiteral("case5-rename") + Theme::instance()->appDotVirtualFileSuffix());
        triggerDownload(fakeFolder, QStringLiteral("case5"));

        // Case 6: foo -> bar (db dehydrate)
        fakeFolder.localModifier().rename(QStringLiteral("case6"), QStringLiteral("case6-rename"));
        markForDehydration(fakeFolder, QStringLiteral("case6"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        // Case 3: the rename went though, hydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case3")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case3-rename")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case3-rename") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("case3")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("case3-rename")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("case3-rename") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("case3-rename") + Theme::instance()->appDotVirtualFileSuffix())._type == ItemTypeVirtualFile);

        // Case 4: the rename went though, dehydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case4")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case4") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case4-rename")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case4-rename") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("case4")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("case4-rename")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("case4-rename")), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("case4-rename"))._type == ItemTypeFile);

        // Case 5: the rename went though, hydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case5")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case5") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case5-rename")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case5-rename") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("case5")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("case5-rename")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("case5-rename") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("case5-rename") + Theme::instance()->appDotVirtualFileSuffix())._type == ItemTypeVirtualFile);

        // Case 6: the rename went though, dehydration is forgotten
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case6")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case6") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("case6-rename")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("case6-rename") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("case6")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("case6-rename")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("case6-rename")), CSYNC_INSTRUCTION_RENAME);
        QVERIFY(dbRecord(fakeFolder, QStringLiteral("case6-rename"))._type == ItemTypeFile);
    }

    // Dehydration via sync works
    void testSyncDehydration()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        setupVfs(fakeFolder);

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        ItemCompletedSpy completeSpy(fakeFolder);
        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        //
        // Mark for dehydration and check
        //

        markForDehydration(fakeFolder, QStringLiteral("A/a1"));

        markForDehydration(fakeFolder, QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        // expect: normal dehydration

        markForDehydration(fakeFolder, QStringLiteral("B/b1"));
        fakeFolder.remoteModifier().remove(QStringLiteral("B/b1"));
        // expect: local removal

        markForDehydration(fakeFolder, QStringLiteral("B/b2"));
        fakeFolder.remoteModifier().rename(QStringLiteral("B/b2"), QStringLiteral("B/b3"));
        // expect: B/b2 is gone, B/b3 is NEW placeholder

        markForDehydration(fakeFolder, QStringLiteral("C/c1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("C/c1"));
        // expect: no dehydration, upload of c1

        markForDehydration(fakeFolder, QStringLiteral("C/c2"));
        fakeFolder.localModifier().appendByte(QStringLiteral("C/c2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("C/c2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("C/c2"));
        // expect: no dehydration, conflict

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        auto isDehydrated = [&](const QString &path) {
            QString placeholder = path + Theme::instance()->appDotVirtualFileSuffix();
            return !fakeFolder.currentLocalState().find(path)
                && fakeFolder.currentLocalState().find(placeholder);
        };
        auto hasDehydratedDbEntries = [&](const QString &path) {
            SyncJournalFileRecord normal, suffix;
            fakeFolder.syncJournal().getFileRecord(path, &normal);
            fakeFolder.syncJournal().getFileRecord(path + Theme::instance()->appDotVirtualFileSuffix(), &suffix);
            return !normal.isValid() && suffix.isValid() && suffix._type == ItemTypeVirtualFile;
        };

        QVERIFY(isDehydrated(QStringLiteral("A/a1")));
        QVERIFY(hasDehydratedDbEntries(QStringLiteral("A/a1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())->_type, ItemTypeVirtualFileDehydration);
        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())->_file, QStringLiteral("A/a1"));
        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix())->_renameTarget,
            QStringLiteral("A/a1") + Theme::instance()->appDotVirtualFileSuffix());
        QVERIFY(isDehydrated(QStringLiteral("A/a2")));
        QVERIFY(hasDehydratedDbEntries(QStringLiteral("A/a2")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_SYNC);
        QCOMPARE(completeSpy.findItem(QStringLiteral("A/a2") + Theme::instance()->appDotVirtualFileSuffix())->_type, ItemTypeVirtualFileDehydration);

        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b1")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("B/b1")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("B/b1")), CSYNC_INSTRUCTION_REMOVE);

        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("B/b2")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("B/b2")));
        QVERIFY(isDehydrated(QStringLiteral("B/b3")));
        QVERIFY(hasDehydratedDbEntries(QStringLiteral("B/b3")));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("B/b2")), CSYNC_INSTRUCTION_REMOVE);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("B/b3") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);

        QCOMPARE(fakeFolder.currentRemoteState().find(QStringLiteral("C/c1"))->contentSize, 25);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("C/c1")), CSYNC_INSTRUCTION_SYNC);

        QCOMPARE(fakeFolder.currentRemoteState().find(QStringLiteral("C/c2"))->contentSize, 26);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("C/c2")), CSYNC_INSTRUCTION_CONFLICT);
        cleanup();

        auto expectedLocalState = fakeFolder.currentLocalState();
        auto expectedRemoteState = fakeFolder.currentRemoteState();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), expectedLocalState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedRemoteState);
    }

    void testSwitchOffVFS_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WithSuffix)) {
            QTest::newRow("Vfs::withSuffix") << Vfs::WithSuffix;
        }

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi") << Vfs::WindowsCfApi;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi, plugin is unavailables");
        }
    }

    void testSwitchOffVFS()
    {
        // This tests the procedure done in Folder::setVirtualFilesEnabled. For changes here, make sure they are reflected in that method.

        QFETCH(Vfs::Mode, vfsMode);
        auto fileName = [vfsMode](const QString &fn) -> QString {
            if (vfsMode == Vfs::WithSuffix) {
                return fn + Theme::instance()->appDotVirtualFileSuffix();
            }

            return fn;
        };

        FakeFolder fakeFolder{ FileInfo{} };
        setupVfs(fakeFolder, vfsMode);

        // Create a suffix-vfs baseline
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A/B"));
        fakeFolder.remoteModifier().insert(QStringLiteral("f1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/B/b1"));
        fakeFolder.localModifier().mkdir(QStringLiteral("A"));
        fakeFolder.localModifier().mkdir(QStringLiteral("A/B"));
        fakeFolder.localModifier().insert(QStringLiteral("f2"));
        fakeFolder.localModifier().insert(QStringLiteral("A/a2"));
        fakeFolder.localModifier().insert(QStringLiteral("A/B/b2"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(fakeFolder.currentLocalState().find(fileName(QStringLiteral("f1"))));
        QVERIFY(fakeFolder.currentLocalState().find(fileName(QStringLiteral("A/a1"))));
        QVERIFY(fakeFolder.currentLocalState().find(fileName(QStringLiteral("A/a3"))));
        QVERIFY(fakeFolder.currentLocalState().find(fileName(QStringLiteral("A/B/b1"))));

        // Make local changes to a3
        fakeFolder.localModifier().remove(fileName(QStringLiteral("A/a3")));
        fakeFolder.localModifier().insert(fileName(QStringLiteral("A/a3")), 100_b);
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());

        // Now wipe the virtuals
        fakeFolder.syncEngine().syncOptions()._vfs->wipeDehydratedVirtualFiles();

        QVERIFY(!fakeFolder.currentLocalState().find(fileName(QStringLiteral("f1"))));
        QVERIFY(!fakeFolder.currentLocalState().find(fileName(QStringLiteral("A/a1"))));
        QVERIFY(fakeFolder.currentLocalState().find(fileName(QStringLiteral("A/a3"))));
        QVERIFY(!fakeFolder.currentLocalState().find(fileName(QStringLiteral("A/B/b1"))));

        fakeFolder.switchToVfs(QSharedPointer<Vfs>(VfsPluginManager::instance().createVfsFromPlugin(Vfs::Off).release()));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        if (vfsMode == Vfs::WithSuffix) {
            // An invalid placeholder was created (one with a suffix, but bigger than 0 bytes), so that will not be synced to the server.
            QVERIFY(!fakeFolder.currentRemoteState().find(fileName(QStringLiteral("A/a3"))));
        } else {
            // After the initial sync, "A/a3" was a dehydrated placeholder. Because of the modifications to "A/a3" above, it should now be a file,
            // and it should not be wiped when the placeholders were removed due to the VFS switch.
            QVERIFY(!fakeFolder.currentRemoteState().find(fileName(QStringLiteral("A/a3")))->isDehydratedPlaceholder);
        }
        QVERIFY(fakeFolder.currentLocalState() != fakeFolder.currentRemoteState());
    }

    void testNewVirtuals()
    {
        FakeFolder fakeFolder{ FileInfo() };
        setupVfs(fakeFolder);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        auto setPin = [&] (const QByteArray &path, PinState state) {
            fakeFolder.syncJournal().internalPinStates().setForPath(path, state);
        };

        fakeFolder.remoteModifier().mkdir(QStringLiteral("local"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("online"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("unspec"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPin("local", PinState::AlwaysLocal);
        setPin("online", PinState::OnlineOnly);
        setPin("unspec", PinState::Unspecified);

        // Test 1: root is Unspecified
        fakeFolder.remoteModifier().insert(QStringLiteral("file1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("online/file1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("local/file1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("unspec/file1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("online/file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("local/file1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("unspec/file1") + Theme::instance()->appDotVirtualFileSuffix()));

        // Test 2: change root to AlwaysLocal
        setPin("", PinState::AlwaysLocal);

        fakeFolder.remoteModifier().insert(QStringLiteral("file2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("online/file2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("local/file2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("unspec/file2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file2")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("online/file2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("local/file2")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("unspec/file2") + Theme::instance()->appDotVirtualFileSuffix()));

        // root file1 was hydrated due to its new pin state
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file1")));

        // file1 is unchanged in the explicitly pinned subfolders
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("online/file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("local/file1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("unspec/file1") + Theme::instance()->appDotVirtualFileSuffix()));

        // Test 3: change root to OnlineOnly
        setPin("", PinState::OnlineOnly);

        fakeFolder.remoteModifier().insert(QStringLiteral("file3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("online/file3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("local/file3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("unspec/file3"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("online/file3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("local/file3")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("unspec/file3") + Theme::instance()->appDotVirtualFileSuffix()));

        // root file1 was dehydrated due to its new pin state
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix()));

        // file1 is unchanged in the explicitly pinned subfolders
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("online/file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("local/file1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("unspec/file1") + Theme::instance()->appDotVirtualFileSuffix()));
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
        fakeFolder.remoteModifier().mkdir(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file1"), 30_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file2"), 40_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file3"), 60_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file3") + Theme::instance()->appDotVirtualFileSuffix(), 70_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file4"), 80_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file4") + Theme::instance()->appDotVirtualFileSuffix(), 90_b, 'A');
        fakeFolder.remoteModifier().insert(
            QStringLiteral("A/file4") + Theme::instance()->appDotVirtualFileSuffix() + Theme::instance()->appDotVirtualFileSuffix(), 100_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file5"), 110_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file6"), 120_b, 'A');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/file1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/file1") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/file2")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/file2") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/file3")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/file3") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/file4")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/file4") + Theme::instance()->appDotVirtualFileSuffix()));
        QVERIFY(!fakeFolder.currentLocalState().find(
            QStringLiteral("A/file4") + Theme::instance()->appDotVirtualFileSuffix() + Theme::instance()->appDotVirtualFileSuffix()));
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/file1") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/file2") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/file3") + Theme::instance()->appDotVirtualFileSuffix()), CSYNC_INSTRUCTION_NEW);
        cleanup();

        // Create odd extra files locally and remotely
        fakeFolder.localModifier().insert(QStringLiteral("A/file1"), 10_b, 'A');
        fakeFolder.localModifier().insert(
            QStringLiteral("A/file2") + Theme::instance()->appDotVirtualFileSuffix() + Theme::instance()->appDotVirtualFileSuffix(), 10_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file5") + Theme::instance()->appDotVirtualFileSuffix(), 10_b, 'A');
        fakeFolder.localModifier().insert(QStringLiteral("A/file6"), 10_b, 'A');
        fakeFolder.remoteModifier().insert(QStringLiteral("A/file6") + Theme::instance()->appDotVirtualFileSuffix(), 10_b, 'A');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/file1")), CSYNC_INSTRUCTION_CONFLICT);
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/file1") + Theme::instance()->appDotVirtualFileSuffix()),
            CSYNC_INSTRUCTION_REMOVE); // it's now a pointless real virtual file
        QCOMPARE(itemInstruction(completeSpy, QStringLiteral("A/file6")), CSYNC_INSTRUCTION_CONFLICT);
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

        fakeFolder.remoteModifier().mkdir(QStringLiteral("local"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("local/sub"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("online"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("online/sub"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("unspec"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPin("local", PinState::AlwaysLocal);
        setPin("online", PinState::OnlineOnly);
        setPin("unspec", PinState::Unspecified);

        fakeFolder.remoteModifier().insert(QStringLiteral("file1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("online/file1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("online/file2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("local/file1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("local/file2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("unspec/file1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        // root is unspecified
        QCOMPARE(*vfs->availability(QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix()), VfsItemAvailability::AllDehydrated);
        QCOMPARE(*vfs->availability(QStringLiteral("local")), VfsItemAvailability::AlwaysLocal);
        QCOMPARE(*vfs->availability(QStringLiteral("local/file1")), VfsItemAvailability::AlwaysLocal);
        QCOMPARE(*vfs->availability(QStringLiteral("online")), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability(QStringLiteral("online/file1") + Theme::instance()->appDotVirtualFileSuffix()), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability(QStringLiteral("unspec")), VfsItemAvailability::AllDehydrated);
        QCOMPARE(*vfs->availability(QStringLiteral("unspec/file1") + Theme::instance()->appDotVirtualFileSuffix()), VfsItemAvailability::AllDehydrated);

        // Subitem pin states can ruin "pure" availabilities
        setPin("local/sub", PinState::OnlineOnly);
        QCOMPARE(*vfs->availability(QStringLiteral("local")), VfsItemAvailability::AllHydrated);
        setPin("online/sub", PinState::Unspecified);
        QCOMPARE(*vfs->availability(QStringLiteral("online")), VfsItemAvailability::AllDehydrated);

        triggerDownload(fakeFolder, QStringLiteral("unspec/file1"));
        setPin("local/file2", PinState::OnlineOnly);
        setPin((QStringLiteral("online/file2") + Theme::instance()->appDotVirtualFileSuffix()).toUtf8(), PinState::AlwaysLocal);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(*vfs->availability(QStringLiteral("unspec")), VfsItemAvailability::AllHydrated);
        QCOMPARE(*vfs->availability(QStringLiteral("local")), VfsItemAvailability::Mixed);
        QCOMPARE(*vfs->availability(QStringLiteral("online")), VfsItemAvailability::Mixed);

        QVERIFY(vfs->setPinState(QStringLiteral("local"), PinState::AlwaysLocal));
        QVERIFY(vfs->setPinState(QStringLiteral("online"), PinState::OnlineOnly));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(*vfs->availability(QStringLiteral("online")), VfsItemAvailability::OnlineOnly);
        QCOMPARE(*vfs->availability(QStringLiteral("local")), VfsItemAvailability::AlwaysLocal);

        auto r = vfs->availability(QStringLiteral("nonexistant"));
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

        fakeFolder.remoteModifier().mkdir(QStringLiteral("local"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("online"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("unspec"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        setPin("local", PinState::AlwaysLocal);
        setPin("online", PinState::OnlineOnly);
        setPin("unspec", PinState::Unspecified);

        fakeFolder.localModifier().insert(QStringLiteral("file1"));
        fakeFolder.localModifier().insert(QStringLiteral("online/file1"));
        fakeFolder.localModifier().insert(QStringLiteral("online/file2"));
        fakeFolder.localModifier().insert(QStringLiteral("local/file1"));
        fakeFolder.localModifier().insert(QStringLiteral("unspec/file1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // root is unspecified
        QCOMPARE(*vfs->pinState(QStringLiteral("file1") + Theme::instance()->appDotVirtualFileSuffix()), PinState::Unspecified);
        QCOMPARE(*vfs->pinState(QStringLiteral("local/file1")), PinState::AlwaysLocal);
        QCOMPARE(*vfs->pinState(QStringLiteral("online/file1")), PinState::Unspecified);
        QCOMPARE(*vfs->pinState(QStringLiteral("unspec/file1")), PinState::Unspecified);

        // Sync again: bad pin states of new local files usually take effect on second sync
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // When a file in an online-only folder is renamed, it retains its pin
        fakeFolder.localModifier().rename(QStringLiteral("online/file1"), QStringLiteral("online/file1rename"));
        fakeFolder.remoteModifier().rename(QStringLiteral("online/file2"), QStringLiteral("online/file2rename"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(*vfs->pinState(QStringLiteral("online/file1rename")), PinState::Unspecified);
        QCOMPARE(*vfs->pinState(QStringLiteral("online/file2rename")), PinState::Unspecified);

        // When a folder is renamed, the pin states inside should be retained
        fakeFolder.localModifier().rename(QStringLiteral("online"), QStringLiteral("onlinerenamed1"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed1")), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed1/file1rename")), PinState::Unspecified);

        fakeFolder.remoteModifier().rename(QStringLiteral("onlinerenamed1"), QStringLiteral("onlinerenamed2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2")), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename")), PinState::Unspecified);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // When a file is deleted and later a new file has the same name, the old pin
        // state isn't preserved.
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename")), PinState::Unspecified);
        fakeFolder.remoteModifier().remove(QStringLiteral("onlinerenamed2/file1rename"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename")), PinState::OnlineOnly);
        fakeFolder.remoteModifier().insert(QStringLiteral("onlinerenamed2/file1rename"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename")), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename") + Theme::instance()->appDotVirtualFileSuffix()), PinState::OnlineOnly);

        // When a file is hydrated or dehydrated due to pin state it retains its pin state
        QVERIFY(vfs->setPinState(QStringLiteral("onlinerenamed2/file1rename") + Theme::instance()->appDotVirtualFileSuffix(), PinState::AlwaysLocal));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("onlinerenamed2/file1rename")));
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename")), PinState::AlwaysLocal);

        QVERIFY(vfs->setPinState(QStringLiteral("onlinerenamed2"), PinState::Unspecified));
        QVERIFY(vfs->setPinState(QStringLiteral("onlinerenamed2/file1rename"), PinState::OnlineOnly));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("onlinerenamed2/file1rename") + Theme::instance()->appDotVirtualFileSuffix()));
        QCOMPARE(*vfs->pinState(QStringLiteral("onlinerenamed2/file1rename") + Theme::instance()->appDotVirtualFileSuffix()), PinState::OnlineOnly);
    }
};

QTEST_GUILESS_MAIN(TestSyncVirtualFiles)
#include "testsyncvirtualfiles.moc"
