/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
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

class TestSyncPlaceholders : public QObject
{
    Q_OBJECT

private slots:
    void testPlaceholderLifecycle_data()
    {
        QTest::addColumn<bool>("doLocalDiscovery");

        QTest::newRow("full local discovery") << true;
        QTest::newRow("skip local discovery") << false;
    }

    void testPlaceholderLifecycle()
    {
        QFETCH(bool, doLocalDiscovery);

        FakeFolder fakeFolder{FileInfo()};
        fakeFolder.syncEngine().account()->setUsePlaceholders(true);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
            if (!doLocalDiscovery)
                fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem);
        };
        cleanup();

        // Create a placeholder for a new remote file
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1", 64);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypePlaceholder);
        cleanup();

        // Another sync doesn't actually lead to changes
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(completeSpy.isEmpty());
        cleanup();

        // Neither does a remote change
        fakeFolder.remoteModifier().appendByte("A/a1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_UPDATE_METADATA));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypePlaceholder);
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._fileSize, 65);
        cleanup();

        // If the local placeholder file is removed, it'll just be recreated
        if (!doLocalDiscovery)
            fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, { "A" });
        fakeFolder.localModifier().remove("A/a1.owncloud");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._type, ItemTypePlaceholder);
        QCOMPARE(dbRecord(fakeFolder, "A/a1")._fileSize, 65);
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
        QVERIFY(itemInstruction(completeSpy, "A/a1m", CSYNC_INSTRUCTION_RENAME));
        QCOMPARE(dbRecord(fakeFolder, "A/a1m")._type, ItemTypePlaceholder);
        cleanup();

        // Remote remove is propagated
        fakeFolder.remoteModifier().remove("A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/a1m.owncloud"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1m"));
        QVERIFY(itemInstruction(completeSpy, "A/a1m", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(!dbRecord(fakeFolder, "A/a1m").isValid());
        cleanup();
    }

    void testWithNormalSync()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setUsePlaceholders(true);
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

        // New files on the remote create placeholders
        fakeFolder.remoteModifier().insert("A/new");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentLocalState().find("A/new"));
        QVERIFY(fakeFolder.currentLocalState().find("A/new.owncloud"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/new"));
        QVERIFY(itemInstruction(completeSpy, "A/new", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(dbRecord(fakeFolder, "A/new")._type, ItemTypePlaceholder);
        cleanup();
    }

    void testPlaceholderDownload()
    {
        FakeFolder fakeFolder{FileInfo()};
        fakeFolder.syncEngine().account()->setUsePlaceholders(true);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        auto triggerDownload = [&](const QByteArray &path) {
            auto &journal = fakeFolder.syncJournal();
            SyncJournalFileRecord record;
            journal.getFileRecord(path, &record);
            if (!record.isValid())
                return;
            record._type = ItemTypePlaceholderDownload;
            journal.setFileRecord(record);
        };

        // Create a placeholder for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().insert("A/a1");
        fakeFolder.remoteModifier().insert("A/a2");
        fakeFolder.remoteModifier().insert("A/a3");
        fakeFolder.remoteModifier().insert("A/a4");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/a1.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a2.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a3.owncloud"));
        QVERIFY(fakeFolder.currentLocalState().find("A/a4.owncloud"));
        cleanup();

        // Download by changing the db entry
        triggerDownload("A/a1");
        triggerDownload("A/a2");
        triggerDownload("A/a3");
        triggerDownload("A/a4");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().remove("A/a3");
        fakeFolder.remoteModifier().rename("A/a4", "A/a4m");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(itemInstruction(completeSpy, "A/a1", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a2", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemInstruction(completeSpy, "A/a3", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a4", CSYNC_INSTRUCTION_REMOVE));
        QVERIFY(itemInstruction(completeSpy, "A/a4m", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestSyncPlaceholders)
#include "testsyncplaceholders.moc"
