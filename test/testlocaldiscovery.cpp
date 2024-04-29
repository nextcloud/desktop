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
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testSelectiveSyncQuotaExceededDataLoss()
    {
        FakeFolder fakeFolder{FileInfo{}};

        // folders that fit the quota
        fakeFolder.localModifier().mkdir("big-files");
        fakeFolder.localModifier().insert("big-files/bigfile_A.data", 1000);
        fakeFolder.localModifier().insert("big-files/bigfile_B.data", 1000);
        fakeFolder.localModifier().insert("big-files/bigfile_C.data", 1000);
        fakeFolder.localModifier().mkdir("more-big-files");
        fakeFolder.localModifier().insert("more-big-files/bigfile_A.data", 1000);
        fakeFolder.localModifier().insert("more-big-files/bigfile_B.data", 1000);
        fakeFolder.localModifier().insert("more-big-files/bigfile_C.data", 1000);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // folders that won't fit
        fakeFolder.localModifier().mkdir("big-files-wont-fit");
        fakeFolder.localModifier().insert("big-files-wont-fit/bigfile_A.data", 800);
        fakeFolder.localModifier().insert("big-files-wont-fit/bigfile_B.data", 800);
        fakeFolder.localModifier().mkdir("more-big-files-wont-fit");
        fakeFolder.localModifier().insert("more-big-files-wont-fit/bigfile_A.data", 800);
        fakeFolder.localModifier().insert("more-big-files-wont-fit/bigfile_B.data", 800);

        const auto remoteQuota = 600;
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)
            if (op == QNetworkAccessManager::PutOperation) {
                if (request.rawHeader("OC-Total-Length").toInt() > remoteQuota) {
                    return new FakeErrorReply(op, request, &parent, 507);
                }
            }
            return nullptr;
        });

        QVERIFY(!fakeFolder.syncOnce());

        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {"big-files-wont-fit/", "more-big-files-wont-fit/"});
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {"big-files-wont-fit/", "more-big-files-wont-fit/"});

        QVERIFY(fakeFolder.syncEngine().journal()->wipeErrorBlacklist());
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("big-files-wont-fit/bigfile_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big-files-wont-fit/bigfile_B.data"));
        QVERIFY(fakeFolder.currentLocalState().find("more-big-files-wont-fit/bigfile_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("more-big-files-wont-fit/bigfile_B.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("big-files-wont-fit/bigfile_A.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("big-files-wont-fit/bigfile_B.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("more-big-files-wont-fit/bigfile_A.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("more-big-files-wont-fit/bigfile_B.data"));

        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {"big-files-wont-fit/", "more-big-files-wont-fit/", "big-files/"});
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {"more-big-files/"});
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {"big-files/", "more-big-files/"});

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("big-files-wont-fit/bigfile_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big-files-wont-fit/bigfile_B.data"));
        QVERIFY(fakeFolder.currentLocalState().find("more-big-files-wont-fit/bigfile_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("more-big-files-wont-fit/bigfile_B.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("big-files-wont-fit/bigfile_A.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("big-files-wont-fit/bigfile_B.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("more-big-files-wont-fit/bigfile_A.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("more-big-files-wont-fit/bigfile_B.data"));

        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {"big-files/", "more-big-files/"});
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {"big-files-wont-fit/", "more-big-files-wont-fit/"});
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {"big-files/", "more-big-files/"});

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("big-files-wont-fit/bigfile_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big-files-wont-fit/bigfile_B.data"));
        QVERIFY(fakeFolder.currentLocalState().find("more-big-files-wont-fit/bigfile_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("more-big-files-wont-fit/bigfile_B.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("big-files-wont-fit/bigfile_A.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("big-files-wont-fit/bigfile_B.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("more-big-files-wont-fit/bigfile_A.data"));
        QVERIFY(!fakeFolder.currentRemoteState().find("more-big-files-wont-fit/bigfile_B.data"));
    }

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
        QVERIFY(fakeFolder.syncJournal().wipeErrorBlacklist() != -1);
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

        fakeFolder.syncEngine().account()->setCapabilities({ { "files",
            QVariantMap { { "blacklisted_files", QVariantList { ".foo", "bar" } } } } });
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

    void testCreateFileWithTrailingSpaces_localAndRemoteTrimmedDoNotExist_renameAndUploadFile()
    {
        FakeFolder fakeFolder{FileInfo{}};
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
        fakeFolder.localModifier().mkdir(QStringLiteral(" with spaces "));

        ItemCompletedSpy completeSpy(fakeFolder);
        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithSpaces1)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces2)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces3)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(QStringLiteral(" with spaces "))->_status, SyncFileItem::Status::FileNameInvalid);

        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces1);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces2);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces3);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces4);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces5);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces6);
        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + QStringLiteral(" with spaces "));

        completeSpy.clear();

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {QStringLiteral("foo"), QStringLiteral("bar"), QStringLiteral("bla"), QStringLiteral("A/foo"), QStringLiteral("A/bar"), QStringLiteral("A/bla")});
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithSpaces1)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces2)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces3)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::Success);
#ifdef Q_OS_WINDOWS
        QCOMPARE(completeSpy.findItem(QStringLiteral(" with spaces "))->_status, SyncFileItem::Status::NormalError);
#else
        QCOMPARE(completeSpy.findItem(QStringLiteral(" with spaces "))->_status, SyncFileItem::Status::Success);
#endif
    }

    void testCreateFileWithTrailingSpaces_remoteDontGetRenamedAutomatically()
    {
        // On Windows we can't create files/folders with leading/trailing spaces locally. So, we have to fail those items. On other OSs - we just sync them down normally.
        FakeFolder fakeFolder{FileInfo()};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces4("A/ foo");
        const QString fileWithSpaces5("A/ bar ");
        const QString fileWithSpaces6("A/bla ");

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
            QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::Success);
        }
    }

    void testCreateFileWithTrailingSpaces_remoteGetRenamedManually()
    {
        // On Windows we can't create files/folders with leading/trailing spaces locally. So, we have to fail those items. On other OSs - we just sync them down normally.
        FakeFolder fakeFolder{FileInfo()};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces4("A/ foo");
        const QString fileWithSpaces5("A/ bar ");
        const QString fileWithSpaces6("A/bla ");

        const QString fileWithoutSpaces4("A/foo");
        const QString fileWithoutSpaces5("A/bar");
        const QString fileWithoutSpaces6("A/bla");

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
            QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::Success);
            QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::Success);
        }

        fakeFolder.remoteModifier().rename(fileWithSpaces4, fileWithoutSpaces4);
        fakeFolder.remoteModifier().rename(fileWithSpaces5, fileWithoutSpaces5);
        fakeFolder.remoteModifier().rename(fileWithSpaces6, fileWithoutSpaces6);

        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithoutSpaces4)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces5)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces6)->_status, SyncFileItem::Status::Success);
    }

    void testCreateFileWithTrailingSpaces_localTrimmedAlsoCreated_dontRenameAutomaticallyAndDontUploadFile()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces(" foo");
        const QString fileTrimmed("foo");

        fakeFolder.localModifier().insert(fileTrimmed);
        fakeFolder.localModifier().insert(fileWithSpaces);

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentRemoteState().find(fileTrimmed));
        QVERIFY(!fakeFolder.currentRemoteState().find(fileWithSpaces));
        QVERIFY(fakeFolder.currentLocalState().find(fileWithSpaces));
        QVERIFY(fakeFolder.currentLocalState().find(fileTrimmed));
    }

    void testCreateFileWithTrailingSpaces_localTrimmedAlsoCreated_dontRenameAutomaticallyAndUploadBothFiles()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces(" foo");
        const QString fileTrimmed("foo");

        fakeFolder.localModifier().insert(fileTrimmed);
        fakeFolder.localModifier().insert(fileWithSpaces);

        fakeFolder.syncEngine().addAcceptedInvalidFileName(fakeFolder.localPath() + fileWithSpaces);

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentRemoteState().find(fileTrimmed));
        QVERIFY(fakeFolder.currentRemoteState().find(fileWithSpaces));
        QVERIFY(fakeFolder.currentLocalState().find(fileWithSpaces));
        QVERIFY(fakeFolder.currentLocalState().find(fileTrimmed));
    }

    void testCreateFileWithTrailingSpaces_localAndRemoteTrimmedExists_renameFile()
    {
        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fileWithSpaces1(" foo");
        const QString fileWithSpaces2(" bar ");
        const QString fileWithSpaces3("bla ");

        fakeFolder.localModifier().insert(fileWithSpaces1);
        fakeFolder.localModifier().insert(fileWithSpaces2);
        fakeFolder.localModifier().insert(fileWithSpaces3);
        fakeFolder.remoteModifier().insert(fileWithSpaces1);
        fakeFolder.remoteModifier().insert(fileWithSpaces2);
        fakeFolder.remoteModifier().insert(fileWithSpaces3);

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        auto expectedState = fakeFolder.currentLocalState();
        qDebug() << expectedState;
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }

    void testBlockInvalidMtimeSyncRemote()
    {
        constexpr auto INVALID_MODTIME1 = 0;
        constexpr auto INVALID_MODTIME2 = -3600;

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString blaFileRootFolder("bla");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString blaFileSubFolder("subfolder/bla");

        fakeFolder.remoteModifier().insert(fooFileRootFolder);
        fakeFolder.remoteModifier().insert(barFileRootFolder);
        fakeFolder.remoteModifier().insert(blaFileRootFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.remoteModifier().insert(fooFileSubFolder);
        fakeFolder.remoteModifier().insert(barFileSubFolder);
        fakeFolder.remoteModifier().insert(blaFileSubFolder);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.remoteModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.remoteModifier().setModTime(blaFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.remoteModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.remoteModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.remoteModifier().setModTime(blaFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));

        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.syncOnce());

        fakeFolder.remoteModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.remoteModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.remoteModifier().setModTime(blaFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.remoteModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.remoteModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.remoteModifier().setModTime(blaFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));

        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.syncOnce());
    }

    void testBlockInvalidMtimeSyncLocal()
    {
        constexpr auto INVALID_MODTIME1 = 0;
        constexpr auto INVALID_MODTIME2 = -3600;

        FakeFolder fakeFolder{FileInfo{}};

        int nGET = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &, QIODevice *) {
            if (op == QNetworkAccessManager::GetOperation)
                ++nGET;
            return nullptr;
        });

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString blaFileRootFolder("bla");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString blaFileSubFolder("subfolder/bla");

        fakeFolder.remoteModifier().insert(fooFileRootFolder);
        fakeFolder.remoteModifier().insert(barFileRootFolder);
        fakeFolder.remoteModifier().insert(blaFileRootFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.remoteModifier().insert(fooFileSubFolder);
        fakeFolder.remoteModifier().insert(barFileSubFolder);
        fakeFolder.remoteModifier().insert(blaFileSubFolder);

        QVERIFY(fakeFolder.syncOnce());
        nGET = 0;

        fakeFolder.localModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.localModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.localModifier().setModTime(blaFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.localModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.localModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));
        fakeFolder.localModifier().setModTime(blaFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME1));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);

        fakeFolder.localModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.localModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.localModifier().setModTime(blaFileRootFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.localModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.localModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));
        fakeFolder.localModifier().setModTime(blaFileSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MODTIME2));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
    }

    void testDoNotSyncInvalidFutureMtime()
    {
        constexpr auto FUTURE_MTIME = 0xFFFFFFFF;
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{FileInfo{}};
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

        fakeFolder.remoteModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.remoteModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.remoteModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.remoteModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.remoteModifier().setModTime(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.remoteModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.localModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.localModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.localModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.localModifier().setModTime(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));

        QVERIFY(!fakeFolder.syncOnce());
    }

    void testInvalidFutureMtimeRecovery()
    {
        constexpr auto FUTURE_MTIME = 0xFFFFFFFF;
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{FileInfo{}};
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
        fakeFolder.localModifier().setModTime(fooFileRootFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(barFileRootFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(fooFileSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(barFileSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));
        fakeFolder.localModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(FUTURE_MTIME));

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        auto expectedState = fakeFolder.currentLocalState();
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }

    void testDiscoverLockChanges()
    {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.syncEngine().account()->setCapabilities({{"activity", QVariantMap{{"apiv2", QVariantList{"filters", "filters-api", "previews", "rich-strings"}}}},
                                                            {"bruteforce", QVariantMap{{"delay", 0}}},
                                                            {"core", QVariantMap{{"pollinterval", 60}, {"webdav-root", "remote.php/webdav"}}},
                                                            {"dav", QVariantMap{{"chunking", "1.0"}}},
                                                            {"files", QVariantMap{{"bigfilechunking", true}, {"blacklisted_files", QVariantList{".htaccess"}},
                                                                                  {"comments", true},
                                                                                  {"directEditing", QVariantMap{{"etag", "c748e8fc588b54fc5af38c4481a19d20"}, {"url", "https://nextcloud.local/ocs/v2.php/apps/files/api/v1/directEditing"}}},
                                                                                  {"locking", "1.0"}}}});

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString fooFileAaaSubFolder("aaa/subfolder/foo");
        const QString barFileAaaSubFolder("aaa/subfolder/bar");

        fakeFolder.remoteModifier().insert(fooFileRootFolder);
        fakeFolder.remoteModifier().insert(barFileRootFolder);
        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("bar"), FileInfo::LockState::FileLocked, 0, QStringLiteral("user1"), {}, QStringLiteral("user1"), 1648046707, 0);

        fakeFolder.remoteModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.remoteModifier().insert(fooFileSubFolder);
        fakeFolder.remoteModifier().insert(barFileSubFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.remoteModifier().insert(fooFileAaaSubFolder);
        fakeFolder.remoteModifier().insert(barFileAaaSubFolder);

        ItemCompletedSpy completeSpy(fakeFolder);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(completeSpy.findItem("bar")->_locked, OCC::SyncFileItem::LockStatus::LockedItem);
        SyncJournalFileRecord fileRecordBefore;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("bar"), &fileRecordBefore));
        QVERIFY(fileRecordBefore._lockstate._locked);

        fakeFolder.remoteModifier().modifyLockState(QStringLiteral("bar"), FileInfo::LockState::FileUnlocked, {}, {}, {}, {}, {}, {});

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem);

        completeSpy.clear();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(completeSpy.findItem("bar")->_locked, OCC::SyncFileItem::LockStatus::UnlockedItem);
        SyncJournalFileRecord fileRecordAfter;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("bar"), &fileRecordAfter));
        QVERIFY(!fileRecordAfter._lockstate._locked);
    }
};

QTEST_GUILESS_MAIN(TestLocalDiscovery)
#include "testlocaldiscovery.moc"
