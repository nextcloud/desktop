/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "localdiscoverytracker.h"
#include "lockwatcher.h"
#include "syncengine.h"
#include "testutils.h"
#include "testutils/syncenginetestutils.h"

#include <QtTest>

using namespace std::chrono_literals;
using namespace OCC;

#ifdef Q_OS_WIN
#include "common/utility_win.h"
// pass combination of FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_DELETE
Utility::Handle makeHandle(const QString &file, int shareMode)
{
    const auto fName = FileSystem::longWinPath(file);
    const wchar_t *wuri = reinterpret_cast<const wchar_t *>(fName.utf16());
    auto handle = CreateFileW(wuri, GENERIC_READ | GENERIC_WRITE, shareMode, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        qWarning() << GetLastError();
    }
    return Utility::Handle(handle);
}
#endif

class TestLockedFiles : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");
        QTest::addColumn<bool>("filesAreDehydrated");

        QTest::newRow("Vfs::Off") << Vfs::Off << false;

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;

            // TODO: the hydrated version will fail due to an issue in the winvfs plugin, so leave it disabled for now.
            // QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi");
        }
    }

    void testBasicLockFileWatcher()
    {
        auto temporaryDir = TestUtils::createTempDir();
        int count = 0;
        QString file;

        LockWatcher watcher;
        watcher.setCheckInterval(std::chrono::milliseconds(50));
        connect(&watcher, &LockWatcher::fileUnlocked, &watcher, [&](const QString &f) { ++count; file = f; });

        const QString tmpFile = temporaryDir.path()
            + QString::fromUtf8("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                "olonglonglonglong/file🐷.txt");
        {
            // use a long file path to ensure we handle that correctly
            QVERIFY(QFileInfo(tmpFile).dir().mkpath(QStringLiteral(".")));
            QFile tmp(tmpFile);
            QVERIFY(tmp.open(QFile::WriteOnly));
            QVERIFY(tmp.write("ownCLoud"));
        }
        QVERIFY(QFile::exists(tmpFile));

        QVERIFY(!FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::Shared));
        watcher.addFile(tmpFile, FileSystem::LockMode::Shared);
        QVERIFY(watcher.contains(tmpFile, FileSystem::LockMode::Shared));

        QEventLoop loop;
        QTimer::singleShot(120ms, &loop, [&] { loop.exit(); });
        loop.exec();

        QCOMPARE(count, 1);
        QCOMPARE(file, tmpFile);
        QVERIFY(!watcher.contains(tmpFile, FileSystem::LockMode::Shared));

#ifdef Q_OS_WIN
        auto h = makeHandle(tmpFile, 0);
        QVERIFY(FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::Shared));
        watcher.addFile(tmpFile, FileSystem::LockMode::Shared);

        count = 0;
        file.clear();
        QThread::msleep(120);
        qApp->processEvents();

        QCOMPARE(count, 0);
        QVERIFY(file.isEmpty());
        QVERIFY(watcher.contains(tmpFile, FileSystem::LockMode::Shared));

        CloseHandle(h);
        QVERIFY(!FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::Shared));

        QThread::msleep(120);
        qApp->processEvents();

        QCOMPARE(count, 1);
        QCOMPARE(file, tmpFile);
        QVERIFY(!watcher.contains(tmpFile, FileSystem::LockMode::Shared));
#endif
        QVERIFY(temporaryDir.remove());
    }

#ifdef Q_OS_WIN
    void testLockedFilePropagation()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);

        // This test can only work when the file is locally available...
        if (filesAreDehydrated) {
            // ... so force hydration in case of dehydrated vfs
            fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
            QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        }

        QStringList seenLockedFiles;
        connect(&fakeFolder.syncEngine(), &SyncEngine::seenLockedFile, &fakeFolder.syncEngine(),
                [&](const QString &file) { seenLockedFiles.append(file); });

        LocalDiscoveryTracker tracker;
        connect(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted, &tracker, &LocalDiscoveryTracker::slotItemCompleted);
        connect(&fakeFolder.syncEngine(), &SyncEngine::finished, &tracker, &LocalDiscoveryTracker::slotSyncFinished);
        auto hasLocalDiscoveryPath = [&](const QString &path) {
            auto &paths = tracker.localDiscoveryPaths();
            return paths.find(path) != paths.end();
        };

        //
        // Local change, attempted upload, but file is locked!
        //
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        tracker.addTouchedPath(QStringLiteral("A/a1"));
        auto h1 = makeHandle(fakeFolder.localPath() + QStringLiteral("A/a1"), 0);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(seenLockedFiles.contains(fakeFolder.localPath() + QStringLiteral("A/a1")));
        QVERIFY(seenLockedFiles.size() == 1);
        QVERIFY(hasLocalDiscoveryPath(QStringLiteral("A/a1")));

        CloseHandle(h1);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        seenLockedFiles.clear();
        QVERIFY(tracker.localDiscoveryPaths().empty());

        //
        // Remote change, attempted download, but file is locked!
        //
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        auto h2 = makeHandle(fakeFolder.localPath() + QStringLiteral("A/a1"), 0);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(seenLockedFiles.contains(fakeFolder.localPath() + QStringLiteral("A/a1")));
        QVERIFY(seenLockedFiles.size() == 1);

        CloseHandle(h2);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
#endif
};

QTEST_GUILESS_MAIN(TestLockedFiles)
#include "testlockedfiles.moc"
