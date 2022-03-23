/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "testutils/syncenginetestutils.h"
#include "testutils/testutils.h"

#include <syncengine.h>
#include <localdiscoverytracker.h>

#include <QtTest>

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
        fakeFolder.remoteModifier().appendByte("C/c1");
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
            { "A/X", "A/X space", "A/X/beta", "foo bar space/touch", "foo/", "zzz", "zzzz" });

        QVERIFY(engine.shouldDiscoverLocally(""));
        QVERIFY(engine.shouldDiscoverLocally("A"));
        QVERIFY(engine.shouldDiscoverLocally("A/X"));
        QVERIFY(!engine.shouldDiscoverLocally("B"));
        QVERIFY(!engine.shouldDiscoverLocally("A B"));
        QVERIFY(!engine.shouldDiscoverLocally("B/X"));
        QVERIFY(engine.shouldDiscoverLocally("foo bar space"));
        QVERIFY(engine.shouldDiscoverLocally("foo"));
        QVERIFY(!engine.shouldDiscoverLocally("foo bar"));
        QVERIFY(!engine.shouldDiscoverLocally("foo bar/touch"));
        // These are within "A/X" so they should be discovered
        QVERIFY(engine.shouldDiscoverLocally("A/X/alpha"));
        QVERIFY(engine.shouldDiscoverLocally("A/X beta"));
        QVERIFY(engine.shouldDiscoverLocally("A/X/Y"));
        QVERIFY(engine.shouldDiscoverLocally("A/X space"));
        QVERIFY(engine.shouldDiscoverLocally("A/X space/alpha"));
        QVERIFY(!engine.shouldDiscoverLocally("A/Xylo/foo"));
        QVERIFY(engine.shouldDiscoverLocally("zzzz/hello"));
        QVERIFY(!engine.shouldDiscoverLocally("zzza/hello"));

        QEXPECT_FAIL("", "There is a possibility of false positives if the set contains a path "
            "which is a prefix, and that prefix is followed by a character less than '/'", Continue);
        QVERIFY(!engine.shouldDiscoverLocally("A/X o"));

        fakeFolder.syncEngine().setLocalDiscoveryOptions(
            LocalDiscoveryStyle::DatabaseAndFilesystem,
            {});

        QVERIFY(!engine.shouldDiscoverLocally(""));
    }

    // Check whether item success and item failure adjusts the
    // tracker correctly.
    void testTrackerItemCompletion()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        LocalDiscoveryTracker tracker;
        connect(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted, &tracker, &LocalDiscoveryTracker::slotItemCompleted);
        connect(&fakeFolder.syncEngine(), &SyncEngine::finished, &tracker, &LocalDiscoveryTracker::slotSyncFinished);
        auto trackerContains = [&](const char *path) {
            return tracker.localDiscoveryPaths().find(path) != tracker.localDiscoveryPaths().end();
        };

        tracker.addTouchedPath("A/spurious");

        fakeFolder.localModifier().insert("A/a3");
        tracker.addTouchedPath("A/a3");

        fakeFolder.localModifier().insert("A/a4");
        fakeFolder.serverErrorPaths().append("A/a4");
        // We're not adding a4 as touched, it's in the same folder as a3 and will be seen.
        // And due to the error it should be added to the explicit list while a3 gets removed.

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentRemoteState().find("A/a3"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a4"));
        QVERIFY(!trackerContains("A/a3"));
        QVERIFY(trackerContains("A/a4"));
        QVERIFY(trackerContains("A/spurious")); // not removed since overall sync not successful

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::FilesystemOnly);
        tracker.startSyncFullDiscovery();
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.currentRemoteState().find("A/a4"));
        QVERIFY(trackerContains("A/a4")); // had an error, still here
        QVERIFY(!trackerContains("A/spurious")); // removed due to full discovery

        fakeFolder.serverErrorPaths().clear();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        tracker.addTouchedPath("A/newspurious"); // will be removed due to successful sync

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentRemoteState().find("A/a4"));
        QVERIFY(tracker.localDiscoveryPaths().empty());
    }

    void testDirectoryAndSubDirectory()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        fakeFolder.localModifier().mkdir("A/newDir");
        fakeFolder.localModifier().mkdir("A/newDir/subDir");
        fakeFolder.localModifier().insert("A/newDir/subDir/file", 10);

        auto expectedState = fakeFolder.currentLocalState();

        // Only "A" was modified according to the file system tracker
        fakeFolder.syncEngine().setLocalDiscoveryOptions(
            LocalDiscoveryStyle::DatabaseAndFilesystem,
            { "A" });

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }

    // Tests the behavior of invalid filename detection
    void testServerBlacklist()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        auto cap = TestUtils::testCapabilities();
        cap.insert({ { "files", QVariantMap { { "blacklisted_files", QVariantList { ".foo", "bar" } } } } });
        fakeFolder.account()->setCapabilities(cap);
        fakeFolder.localModifier().insert("C/.foo");
        fakeFolder.localModifier().insert("C/bar");
        fakeFolder.localModifier().insert("C/moo");
        fakeFolder.localModifier().insert("C/.moo");

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("C/moo"));
        QVERIFY(fakeFolder.currentRemoteState().find("C/.moo"));
        QVERIFY(!fakeFolder.currentRemoteState().find("C/.foo"));
        QVERIFY(!fakeFolder.currentRemoteState().find("C/bar"));
    }
};

QTEST_GUILESS_MAIN(TestLocalDiscovery)
#include "testlocaldiscovery.moc"
