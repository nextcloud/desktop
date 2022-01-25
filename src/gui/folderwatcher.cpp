/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

// event masks
#include "folderwatcher.h"

#include <stdint.h>

#include <QFileInfo>
#include <QFlags>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#if defined(Q_OS_WIN)
#include "folderwatcher_win.h"
#elif defined(Q_OS_MAC)
#include "folderwatcher_mac.h"
#elif defined(Q_OS_UNIX)
#include "folderwatcher_linux.h"
#endif

#include "folder.h"
#include "filesystem.h"

using namespace std::chrono_literals;

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderWatcher, "gui.folderwatcher", QtInfoMsg)

FolderWatcher::FolderWatcher(Folder *folder)
    : QObject(folder)
    , _folder(folder)
{
}

FolderWatcher::~FolderWatcher()
{
}

void FolderWatcher::init(const QString &root)
{
    _d.reset(new FolderWatcherPrivate(this, root));
    _timer.start();
}

bool FolderWatcher::pathIsIgnored(const QString &path)
{
    if (path.isEmpty())
        return true;
    if (!_folder)
        return false;
    if (_folder->isFileExcludedAbsolute(path) && !Utility::isConflictFile(path)) {
        qCDebug(lcFolderWatcher) << "* Ignoring file" << path;
        return true;
    }
    return false;
}

bool FolderWatcher::isReliable() const
{
    return _isReliable;
}

void FolderWatcher::startNotificatonTest(const QString &path)
{
#ifdef Q_OS_MAC
    // Testing the folder watcher on OSX is harder because the watcher
    // automatically discards changes that were performed by our process.
    // It would still be useful to test but the OSX implementation
    // is deferred until later.
    return;
#endif

    Q_ASSERT(_testNotificationPath.isEmpty());
    Q_ASSERT(!path.isEmpty());
    _testNotificationPath = path;

    // Don't do the local file modification immediately:
    // wait for FolderWatchPrivate::_ready
    startNotificationTestWhenReady();
}

void FolderWatcher::startNotificationTestWhenReady()
{
    if (!_testNotificationPath.isEmpty()) {
        // we already received the notification
        return;
    }
    if (!_d->isReady()) {
        QTimer::singleShot(1s, this, &FolderWatcher::startNotificationTestWhenReady);
        return;
    }

    if (OC_ENSURE(QFile::exists(_testNotificationPath))) {
        const auto mtime = FileSystem::getModTime(_testNotificationPath);
        FileSystem::setModTime(_testNotificationPath, mtime + 1);
    } else {
        QFile f(_testNotificationPath);
        f.open(QIODevice::WriteOnly | QIODevice::Append);
    }

    QTimer::singleShot(5s, this, [this]() {
        if (!_testNotificationPath.isEmpty())
            emit becameUnreliable(tr("The watcher did not receive a test notification."));
        _testNotificationPath.clear();
    });
}


int FolderWatcher::testLinuxWatchCount() const
{
#ifdef Q_OS_LINUX
    return _d->testWatchCount();
#else
    return -1;
#endif
}

void FolderWatcher::changeDetected(const QString &path)
{
    QStringList paths(path);
    changeDetected(paths);
}

void FolderWatcher::changeDetected(const QStringList &paths)
{
    // TODO: this shortcut doesn't look very reliable:
    //   - why is the timeout only 1 second?
    //   - what if there is more than one file being updated frequently?
    //   - why do we skip the file altogether instead of e.g. reducing the upload frequency?

    // Check if the same path was reported within the last second.
    const QSet<QString> pathsSet = paths.toSet();
    if (pathsSet == _lastPaths && _timer.elapsed() < 1000) {
        // the same path was reported within the last second. Skip.
        return;
    }
    _lastPaths = pathsSet;
    _timer.restart();

    // ------- handle ignores:
    for (const auto &path : pathsSet) {
        if (!_testNotificationPath.isEmpty()
            && Utility::fileNamesEqual(path, _testNotificationPath)) {
            _testNotificationPath.clear();
        }
        if (pathIsIgnored(path)) {
            continue;
        }

        qCInfo(lcFolderWatcher) << "Detected changes in paths:" << path;
        emit pathChanged(path);
    }
}

} // namespace OCC
