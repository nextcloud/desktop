/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QDir>
#include <QStandardPaths>
#include <QTest>
#include <QThreadPool>

#include "discoveryphase.h"
#include "localdiscoverytracker.h"
#include "lockwatcher.h"
#include "syncengine.h"
#include "syncenginetestutils.h"

using namespace OCC;

#ifdef Q_OS_WIN
// pass combination of FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_DELETE
HANDLE makeHandle(const QString &file, int shareMode)
{
    const auto fName = FileSystem::longWinPath(file);
    const auto wuri = reinterpret_cast<const wchar_t *>(fName.utf16());
    auto handle = CreateFileW(
        wuri,
        GENERIC_READ | GENERIC_WRITE,
        shareMode,
        nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        qWarning() << GetLastError();
    }
    return handle;
}

// Same as makeHandle(), but FILE_FLAG_BACKUP_SEMANTICS is required to open a directory.
HANDLE makeDirectoryHandle(const QString &directory, int shareMode)
{
    const auto fName = FileSystem::longWinPath(directory);
    auto handle = CreateFileW(
        reinterpret_cast<const wchar_t *>(fName.utf16()),
        GENERIC_READ,
        shareMode,
        nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        qWarning() << GetLastError();
    }
    return handle;
}
#endif

class TestLockedFiles : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

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

        QVERIFY(!FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::SharedRead));
        watcher.addFile(tmpFile);
        QVERIFY(watcher.contains(tmpFile));

        QEventLoop loop;
        QTimer::singleShot(120, &loop, [&] { loop.exit(); });
        loop.exec();

        QCOMPARE(count, 1);
        QCOMPARE(file, tmpFile);
        QVERIFY(!watcher.contains(tmpFile));

#ifdef Q_OS_WIN
        auto h = makeHandle(tmpFile, 0);
        QVERIFY(FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::SharedRead));
        watcher.addFile(tmpFile);

        count = 0;
        file.clear();
        QThread::msleep(120);
        qApp->processEvents();

        QCOMPARE(count, 0);
        QVERIFY(file.isEmpty());
        QVERIFY(watcher.contains(tmpFile));

        CloseHandle(h);
        QVERIFY(!FileSystem::isFileLocked(tmpFile, FileSystem::LockMode::SharedRead));

        QThread::msleep(120);
        qApp->processEvents();

        QCOMPARE(count, 1);
        QCOMPARE(file, tmpFile);
        QVERIFY(!watcher.contains(tmpFile));
#endif
        QVERIFY(tmp.remove());
    }

    // Functional check for local directory discovery #10535: DiscoverySingleLocalDirectoryJob
    // must return every regular file and subdirectory with its name and flags intact.
    void testLocalDirectoryDiscoveryReturnsAllEntries()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QStringList expectedFiles;
        for (int i = 0; i < 50; ++i) {
            // Varied lengths and non ascii, matching the discovery concat path.
            const QString name = QStringLiteral("entry_%1_ααβγ_%2.txt").arg(i).arg(QString(i % 20, QChar('x')));
            QFile file(tmp.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("data");
            expectedFiles.append(name);
        }
        QVERIFY(QDir(tmp.path()).mkdir(QStringLiteral("subdir")));

        const auto job = new DiscoverySingleLocalDirectoryJob({}, tmp.path(), nullptr, false);
        QSignalSpy finishedSpy(job, &DiscoverySingleLocalDirectoryJob::finished);
        QThreadPool::globalInstance()->start(job);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

        const auto results = finishedSpy.takeFirst().at(0).value<QVector<OCC::LocalInfo>>();
        QCOMPARE(results.size(), expectedFiles.size() + 1);

        QStringList seenFiles;
        for (const auto &info : results) {
            QVERIFY(!info.name.isEmpty());
            if (info.isDirectory) {
                QCOMPARE(info.name, QStringLiteral("subdir"));
                continue;
            }
            QVERIFY(!info.isLocked);
            seenFiles.append(info.name);
        }
        seenFiles.sort();
        expectedFiles.sort();
        QCOMPARE(seenFiles, expectedFiles);
    }

#ifdef Q_OS_WIN
    void testLockDetectionUsesRealFileSystemCheck()
    {
        // Regression guard for #10464: exercise the real FileSystem::isFileLocked path
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        for (const auto &name : { QStringLiteral("locked.bin"), QStringLiteral("test.txt") }) {
            QFile tmpFile(tmp.filePath(name));
            QVERIFY(tmpFile.open(QIODevice::WriteOnly));
            tmpFile.write("x");
        }
        QVERIFY(QDir(tmp.path()).mkdir(QStringLiteral("subdir")));

        auto handle = makeHandle(tmp.filePath(QStringLiteral("locked.bin")), 0);
        QVERIFY(handle != INVALID_HANDLE_VALUE);

        const auto job = new DiscoverySingleLocalDirectoryJob({}, tmp.path(), nullptr, false);
        QSignalSpy finishedSpy(job, &DiscoverySingleLocalDirectoryJob::finished);
        QThreadPool::globalInstance()->start(job);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

        CloseHandle(handle);

        const auto results = finishedSpy.takeFirst().at(0).value<QVector<OCC::LocalInfo>>();
        QCOMPARE(results.size(), 3);
        for (const auto &info : results) {
            if (info.name == QStringLiteral("locked.bin")) {
                QVERIFY(info.isLocked);
                continue;
            }

            QVERIFY(!info.isLocked);
            if (info.name == QStringLiteral("subdir")) {
                QVERIFY(info.isDirectory);
            }
        }
    }

    void testDirectoryLockChecks()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const auto subDirectory = tmp.path() + QStringLiteral("/aDirectory");
        QVERIFY(QDir().mkpath(subDirectory));

        const auto fileInDirectory = subDirectory + QStringLiteral("/aFile.txt");
        {
            QFile file(fileInDirectory);
            QVERIFY(file.open(QFile::WriteOnly));
            QVERIFY(file.write("Nextcloud"));
        }

        const auto fileHandle = makeHandle(fileInDirectory, 0);
        QVERIFY(fileHandle != INVALID_HANDLE_VALUE);

        // Logger only forwards fatal messages, so QTest::failOnWarning() would not see
        // these. Count by category, which survives rewording of the message.
        static int warningCount = 0;
        static QStringList warningMessages;
        warningCount = 0;
        warningMessages.clear();
        const auto previousHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
            if (type == QtWarningMsg && context.category && qstrcmp(context.category, "nextcloud.sync.filesystem") == 0) {
                ++warningCount;
                warningMessages.append(message);
            }
        });

        // Every mode has to stay silent, but only SharedRead is asserted on: Exclusive
        // requests deny-sharing, so any unrelated handle an indexer or virus scanner
        // holds would rightfully make it report the directory as locked.
        QVarLengthArray<bool, 2> sharedReadResults;
        for (const auto mode : {FileSystem::LockMode::Shared, FileSystem::LockMode::SharedRead, FileSystem::LockMode::Exclusive}) {
            const auto subDirectoryLocked = FileSystem::isFileLocked(subDirectory, mode);
            const auto rootDirectoryLocked = FileSystem::isFileLocked(tmp.path(), mode);
            if (mode == FileSystem::LockMode::SharedRead) {
                sharedReadResults.append(subDirectoryLocked);
                sharedReadResults.append(rootDirectoryLocked);
            }
        }

        // Skipping the lock on directories must not hide a locked file inside one.
        const auto lockedFileDetected = FileSystem::isFileLocked(fileInDirectory, FileSystem::LockMode::SharedRead);

        // A directory held with deny-sharing is still locked; CreateFileW reports that.
        const auto directoryHandle = makeDirectoryHandle(subDirectory, 0);
        const auto sharedDirectoryDetected = directoryHandle != INVALID_HANDLE_VALUE
            && FileSystem::isFileLocked(subDirectory, FileSystem::LockMode::SharedRead);

        qInstallMessageHandler(previousHandler);
        if (directoryHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(directoryHandle);
        }
        CloseHandle(fileHandle);

        // The failing LockFile() call used to log one warning per directory per run.
        QVERIFY2(warningCount == 12, qPrintable(warningMessages.join(QStringLiteral(" || "))));
        for (const auto isLocked : sharedReadResults) {
            QVERIFY(!isLocked);
        }
        QVERIFY(lockedFileDetected);
        QVERIFY(sharedDirectoryDetected);
        QVERIFY(!FileSystem::isFileLocked(fileInDirectory, FileSystem::LockMode::SharedRead));
    }

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
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(seenLockedFiles.contains(fakeFolder.localPath() + "A/a1"));
        QVERIFY(seenLockedFiles.size() == 1);
        QVERIFY(!hasLocalDiscoveryPath("A/a1"));

        CloseHandle(h1);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {"A/a1"});
        tracker.startSyncPartialDiscovery();
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
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
#endif
};

QTEST_GUILESS_MAIN(TestLockedFiles)
#include "testlockedfiles.moc"
