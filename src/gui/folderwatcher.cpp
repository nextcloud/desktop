/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// event masks
#include "folderwatcher.h"

#include "accountstate.h"
#include "account.h"
#include "capabilities.h"

#if defined(Q_OS_WIN)
#include "folderwatcher_win.h"
#elif defined(Q_OS_MACOS)
#include "folderwatcher_mac.h"
#elif defined(Q_OS_UNIX)
#include "folderwatcher_linux.h"
#endif

#include "folder.h"
#include "filesystem.h"

#include <QFileInfo>
#include <QFlags>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>

#include <array>
#include <cstdint>

namespace
{
constexpr auto lockChangeDebouncingTimerIntervalMs = 500;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderWatcher, "nextcloud.gui.folderwatcher", QtInfoMsg)

FolderWatcher::FolderWatcher(Folder *folder)
    : QObject(folder)
    , _folder(folder)
{
    _lockChangeDebouncingTimer.setInterval(lockChangeDebouncingTimerIntervalMs);

    if (_folder && _folder->accountState() && _folder->accountState()->account()) {
        connect(_folder->accountState()->account().data(), &Account::capabilitiesChanged, this, &FolderWatcher::folderAccountCapabilitiesChanged);
        folderAccountCapabilitiesChanged();
    }
}

FolderWatcher::~FolderWatcher() = default;

void FolderWatcher::init(const QString &root)
{
    _d.reset(new FolderWatcherPrivate(this, root));
    _timer.start();
}

bool FolderWatcher::pathIsIgnored(const QString &path) const
{
    return path.isEmpty();
}

bool FolderWatcher::isReliable() const
{
    return _isReliable;
}

bool FolderWatcher::canSetPermissions() const
{
    return _canSetPermissions;
}

void FolderWatcher::appendSubPaths(QDir dir, QStringList &subPaths)
{
    QStringList newSubPaths = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    for (int i = 0; i < newSubPaths.size(); i++) {
        QString path = dir.path() + "/" + newSubPaths[i];
        QFileInfo fileInfo(path);
        subPaths.append(path);
        if (FileSystem::isDir(path)) {
            QDir dir(path);
            appendSubPaths(dir, subPaths);
        }
    }
}

void FolderWatcher::startNotificatonTest(const QString &path)
{
#ifdef Q_OS_MACOS
    // Testing the folder watcher on OSX is harder because the watcher
    // automatically discards changes that were performed by our process.
    // It would still be useful to test but the OSX implementation
    // is deferred until later.
    return;
#endif

    Q_ASSERT(_testNotificationPath.isEmpty());
    _testNotificationPath = path;

    // Don't do the local file modification immediately:
    // wait for FolderWatchPrivate::_ready
    startNotificationTestWhenReady();
}

void FolderWatcher::performSetPermissionsTest(const QString &path)
{
    _canSetPermissions = true;

    if (!QFile::exists(path)) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            qCWarning(lcFolderWatcher()) << "Failed to create test file: " << path;
            return;
        }
        f.write("test");
        f.close();
    }

    if (!FileSystem::isWritable(path)) {
        FileSystem::setFileReadOnly(path, false);
        if (!FileSystem::isWritable(path)) {
            qCWarning(lcFolderWatcher()) << "Cannot make file readable: " << path;
            _canSetPermissions = false;
            QFile(path).remove();
            return;
        }
    }

    FileSystem::setFileReadOnly(path, true);
    if (FileSystem::isWritable(path)) {
        qCWarning(lcFolderWatcher()) << "Cannot make file read-only: " << path;
        _canSetPermissions = false;
    }

    qCInfo(lcFolderWatcher()) << "Permissions in file system for" << path << (_canSetPermissions ? "works as expected" : "are not reliable");

    FileSystem::setFileReadOnly(path, false);
    QFile(path).remove();
}

void FolderWatcher::startNotificationTestWhenReady()
{
    if (!_d->_ready) {
        QTimer::singleShot(1000, this, &FolderWatcher::startNotificationTestWhenReady);
        return;
    }

    auto path = _testNotificationPath;
    if (QFile::exists(path)) {
        auto mtime = FileSystem::getModTime(path);
        qCDebug(lcFolderWatcher()) << "setModTime" << path << (mtime + 1);
        FileSystem::setModTime(path, mtime + 1);
    } else {
        QFile f(path);
        [[maybe_unused]] bool opened = f.open(QIODevice::WriteOnly | QIODevice::Append);
    }
    FileSystem::setFileHidden(path, true);

    QTimer::singleShot(5000, this, [this]() {
        if (!_testNotificationPath.isEmpty())
            emit becameUnreliable(tr("The watcher did not receive a test notification."));
        _testNotificationPath.clear();
    });
}

void FolderWatcher::lockChangeDebouncingTimerTimedOut()
{
    if (!_unlockedFiles.isEmpty()) {
        const auto unlockedFilesCopy = _unlockedFiles;
        emit filesLockReleased(unlockedFilesCopy);
        _unlockedFiles.clear();
    }
    if (!_lockedFiles.isEmpty()) {
        const auto lockedFilesCopy = _lockedFiles;
        emit filesLockImposed(lockedFilesCopy);
        emit lockedFilesFound(lockedFilesCopy);
        _lockedFiles.clear();
    }
}

int FolderWatcher::testLinuxWatchCount() const
{
#ifdef Q_OS_LINUX
    return _d->testWatchCount();
#else
    return -1;
#endif
}

void FolderWatcher::slotLockFileDetectedExternally(const QString &lockFile)
{
    qCInfo(lcFolderWatcher) << "Lock file detected externally, probably a newly-uploaded office file: " << lockFile;
    changeDetected(lockFile);
}

void FolderWatcher::setShouldWatchForFileUnlocking(bool shouldWatchForFileUnlocking)
{
    _shouldWatchForFileUnlocking = shouldWatchForFileUnlocking;
}

int FolderWatcher::lockChangeDebouncingTimout() const
{
    return _lockChangeDebouncingTimer.interval();
}

void FolderWatcher::changeDetected(const QString &path)
{
    QStringList paths(path);
    if (FileSystem::isDir(path)) {
        QDir dir(path);
        appendSubPaths(dir, paths);
    }
    changeDetected(paths);
}

void FolderWatcher::changeDetected(const QStringList &paths)
{
    // TODO: this shortcut doesn't look very reliable:
    //   - why is the timeout only 1 second?
    //   - what if there is more than one file being updated frequently?
    //   - why do we skip the file altogether instead of e.g. reducing the upload frequency?

    // Check if the same path was reported within the last second.
    const auto pathsSet = QSet<QString>{paths.begin(), paths.end()};
    if (pathsSet == _lastPaths && _timer.elapsed() < 1000) {
        // the same path was reported within the last second. Skip.
        return;
    }
    _lastPaths = pathsSet;
    _timer.restart();

    QSet<QString> changedPaths;

    for (const auto &path : paths) {
        if (!_testNotificationPath.isEmpty()
            && Utility::fileNamesEqual(path, _testNotificationPath)) {
            _testNotificationPath.clear();
        }

        const auto lockFileNamePattern = FileSystem::filePathLockFilePatternMatch(path);
        const auto checkResult = FileSystem::lockFileTargetFilePath(path, lockFileNamePattern);
        if (_shouldWatchForFileUnlocking) {
            // Lock file has been deleted, file now unlocked
            if (checkResult.type == FileSystem::FileLockingInfo::Type::Unlocked && !checkResult.path.isEmpty()) {
                _lockedFiles.remove(checkResult.path);
                _unlockedFiles.insert(checkResult.path);
            }
        }

        if (checkResult.type == FileSystem::FileLockingInfo::Type::Locked && !checkResult.path.isEmpty()) {
            _unlockedFiles.remove(checkResult.path);
            _lockedFiles.insert(checkResult.path);
        }

        // ------- handle ignores:
        if (pathIsIgnored(path)) {
            continue;
        }

        changedPaths.insert(path);
    }

    if (!_lockedFiles.isEmpty() || !_unlockedFiles.isEmpty()) {
        if (_lockChangeDebouncingTimer.isActive()) {
            _lockChangeDebouncingTimer.stop();
        }
        _lockChangeDebouncingTimer.setSingleShot(true);
        _lockChangeDebouncingTimer.start();
        _lockChangeDebouncingTimer.connect(&_lockChangeDebouncingTimer, &QTimer::timeout, this, &FolderWatcher::lockChangeDebouncingTimerTimedOut, Qt::UniqueConnection);
    }

    if (changedPaths.isEmpty()) {
        return;
    }

    for (const auto &path : changedPaths) {
        emit pathChanged(path);
    }
}

void FolderWatcher::folderAccountCapabilitiesChanged()
{
    _shouldWatchForFileUnlocking = _folder->accountState()->account()->capabilities().filesLockAvailable();
}

} // namespace OCC
