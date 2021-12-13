/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "testutils/syncenginetestutils.h"
#include "lockwatcher.h"
#include <syncengine.h>
#include <localdiscoverytracker.h>

using namespace std::chrono_literals;
using namespace OCC;

#ifdef Q_OS_WIN
// pass combination of FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_DELETE
HANDLE makeHandle(const QString &file, int shareMode)
{
    const auto fName = FileSystem::longWinPath(file);
    const wchar_t *wuri = reinterpret_cast<const wchar_t *>(fName.utf16());
    auto handle = CreateFileW(
        wuri,
        GENERIC_READ | GENERIC_WRITE,
        shareMode,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        qWarning() << GetLastError();
    }
    return handle;
}
#endif

class TestLockedFiles : public QObject
{
    Q_OBJECT

private slots:
    void testBasicLockFileWatcher()
    {
        QTemporaryDir tmp;
        int count = 0;
        QString file;

        LockWatcher watcher;
        watcher.setCheckInterval(std::chrono::milliseconds(50));
        connect(&watcher, &LockWatcher::fileUnlocked, &watcher, [&](const QString &f) { ++count; file = f; });

        const QString tmpFile = tmp.path() + QString::fromUtf8("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                                               "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                                               "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                                               "olonglonglonglong/file🐷.txt");
        {
            // use a long file path to ensure we handle that correctly
            QVERIFY(QFileInfo(tmpFile).dir().mkpath("."));
            QFile tmp(tmpFile);
            QVERIFY(tmp.open(QFile::WriteOnly));
            QVERIFY(tmp.write("ownCLoud"));
        }
        QVERIFY(QFile::exists(tmpFile));

        QVERIFY(!FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::Shared));
        watcher.addFile(tmpFile, FileSystem::LockMode::Shared);
        QVERIFY(watcher.contains(tmpFile));

        QEventLoop loop;
        QTimer::singleShot(120ms, &loop, [&] { loop.exit(); });
        loop.exec();

        QCOMPARE(count, 1);
        QCOMPARE(file, tmpFile);
        QVERIFY(!watcher.contains(tmpFile));

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
        QVERIFY(watcher.contains(tmpFile));

        CloseHandle(h);
        QVERIFY(!FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::Shared));

        QThread::msleep(120);
        qApp->processEvents();

        QCOMPARE(count, 1);
        QCOMPARE(file, tmpFile);
        QVERIFY(!watcher.contains(tmpFile));
#endif
        QVERIFY(tmp.remove());
    }

#ifdef Q_OS_WIN
    void testLockedFilePropagation()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        QStringList seenLockedFiles;
        connect(&fakeFolder.syncEngine(), &SyncEngine::seenLockedFile, &fakeFolder.syncEngine(),
                [&](const QString &file) { seenLockedFiles.append(file); });

        LocalDiscoveryTracker tracker;
        connect(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted, &tracker, &LocalDiscoveryTracker::slotItemCompleted);
        connect(&fakeFolder.syncEngine(), &SyncEngine::finished, &tracker, &LocalDiscoveryTracker::slotSyncFinished);
        auto hasLocalDiscoveryPath = [&](const QString &path) {
            auto &paths = tracker.localDiscoveryPaths();
            return paths.find(path.toUtf8()) != paths.end();
        };

        //
        // Local change, attempted upload, but file is locked!
        //
        fakeFolder.localModifier().appendByte("A/a1");
        tracker.addTouchedPath("A/a1");
        auto h1 = makeHandle(fakeFolder.localPath() + "A/a1", 0);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(seenLockedFiles.contains(fakeFolder.localPath() + "A/a1"));
        QVERIFY(seenLockedFiles.size() == 1);
        QVERIFY(hasLocalDiscoveryPath("A/a1"));

        CloseHandle(h1);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        seenLockedFiles.clear();
        QVERIFY(tracker.localDiscoveryPaths().empty());

        //
        // Remote change, attempted download, but file is locked!
        //
        fakeFolder.remoteModifier().appendByte("A/a1");
        auto h2 = makeHandle(fakeFolder.localPath() + "A/a1", 0);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(seenLockedFiles.contains(fakeFolder.localPath() + "A/a1"));
        QVERIFY(seenLockedFiles.size() == 1);

        CloseHandle(h2);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, tracker.localDiscoveryPaths());
        tracker.startSyncPartialDiscovery();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
#endif
};

QTEST_GUILESS_MAIN(TestLockedFiles)
#include "testlockedfiles.moc"
