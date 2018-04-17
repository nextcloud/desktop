/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>
#include <localdiscoverytracker.h>

using namespace OCC;

class TestLocalDiscovery : public QObject
{
    Q_OBJECT

private slots:
    // Check correct behavior when local discovery is partially drawn from the db
    void testLocalDiscoveryStyle()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        LocalDiscoveryTracker tracker;
        connect(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted, &tracker, &LocalDiscoveryTracker::slotItemCompleted);
        connect(&fakeFolder.syncEngine(), &SyncEngine::finished, &tracker, &LocalDiscoveryTracker::slotSyncFinished);

        // More subdirectories are useful for testing
        fakeFolder.localModifier().mkdir("A/X");
        fakeFolder.localModifier().mkdir("A/Y");
        fakeFolder.localModifier().insert("A/X/x1");
        fakeFolder.localModifier().insert("A/Y/y1");
        tracker.addTouchedPath("A/X");

        tracker.startSyncFullDiscovery();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(tracker.localDiscoveryPaths().empty());

        // Test begins
        fakeFolder.localModifier().insert("A/a3");
        fakeFolder.localModifier().insert("A/X/x2");
        fakeFolder.localModifier().insert("A/Y/y2");
        fakeFolder.localModifier().insert("B/b3");
        fakeFolder.remoteModifier().insert("C/c3");

        tracker.addTouchedPath("A/X");
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());

        tracker.startSyncPartialDiscovery();
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentRemoteState().find("A/a3"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/X/x2"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/Y/y2"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b3"));
        QVERIFY(fakeFolder.currentLocalState().find("C/c3"));
        QCOMPARE(fakeFolder.syncEngine().lastLocalDiscoveryStyle(), LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(tracker.localDiscoveryPaths().empty());

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.syncEngine().lastLocalDiscoveryStyle(), LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(tracker.localDiscoveryPaths().empty());
    }

    void testLocalDiscoveryDecision()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto &engine = fakeFolder.syncEngine();

        QVERIFY(engine.shouldDiscoverLocally(""));
        QVERIFY(engine.shouldDiscoverLocally("A"));
        QVERIFY(engine.shouldDiscoverLocally("A/X"));

        fakeFolder.syncEngine().setLocalDiscoveryOptions(
            LocalDiscoveryStyle::DatabaseAndFilesystem,
            { "A/X", "foo bar space/touch", "foo/", "zzz" });

        QVERIFY(engine.shouldDiscoverLocally(""));
        QVERIFY(engine.shouldDiscoverLocally("A"));
        QVERIFY(engine.shouldDiscoverLocally("A/X"));
        QVERIFY(!engine.shouldDiscoverLocally("B"));
        QVERIFY(!engine.shouldDiscoverLocally("A B"));
        QVERIFY(!engine.shouldDiscoverLocally("B/X"));
        QVERIFY(!engine.shouldDiscoverLocally("A/X/Y"));
        QVERIFY(engine.shouldDiscoverLocally("foo bar space"));
        QVERIFY(engine.shouldDiscoverLocally("foo"));
        QVERIFY(!engine.shouldDiscoverLocally("foo bar"));
        QVERIFY(!engine.shouldDiscoverLocally("foo bar/touch"));

        fakeFolder.syncEngine().setLocalDiscoveryOptions(
            LocalDiscoveryStyle::DatabaseAndFilesystem,
            {});

        QVERIFY(!engine.shouldDiscoverLocally(""));
    }
};

QTEST_GUILESS_MAIN(TestLocalDiscovery)
#include "testlocaldiscovery.moc"
