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

#include <cerrno>
#include <sys/inotify.h>
#include <unistd.h>

#include "folder.h"
#include "folderwatcher_linux.h"

#include <QObject>
#include <QStringList>
#include <QVarLengthArray>

namespace OCC {

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString &path)
    : QObject()
    , _parent(p)
    , _folder(path)
{
    _fd = inotify_init();
    if (_fd != -1) {
        _socket.reset(new QSocketNotifier(_fd, QSocketNotifier::Read));
        connect(_socket.data(), &QSocketNotifier::activated, this, &FolderWatcherPrivate::slotReceivedNotification);
    } else {
        qCWarning(lcFolderWatcher) << "notify_init() failed: " << strerror(errno);
    }

    QMetaObject::invokeMethod(this, [path, this] { slotAddFolderRecursive(path); });
}

// attention: result list passed by reference!
bool FolderWatcherPrivate::findFoldersBelow(const QDir &dir, QStringList &fullList)
{
    if (!dir.exists()) {
        qCDebug(lcFolderWatcher) << "      - non existing path coming in: " << dir.absolutePath();
        return false;
    } else if (!dir.isReadable()) {
        qCDebug(lcFolderWatcher) << "      - path without read permissions coming in: " << dir.absolutePath();
        return false;
    }

    const QDir::Filters filter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden;

    bool ok = true;
    for (const QString &path : dir.entryList({ QStringLiteral("*") }, filter)) {
        const QString fullPath(dir.path() + QLatin1String("/") + path);
        fullList.append(fullPath);
        ok &= findFoldersBelow(QDir(fullPath), fullList);
    }
    return ok;
}

void FolderWatcherPrivate::inotifyRegisterPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    int wd = inotify_add_watch(_fd, path.toUtf8().constData(),
        IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR);
    if (wd != -1) {
        _watchToPath.insert(wd, path);
        _pathToWatch.insert(path, wd);
    } else {
        // If we're running out of memory or inotify watches, become unreliable.
        if (_parent->_isReliable && (errno == ENOMEM || errno == ENOSPC)) {
            _parent->_isReliable = false;
            emit _parent->becameUnreliable(
                tr("This problem usually happens when the inotify watches are exhausted. "
                   "Check the FAQ for details."));
        }
    }
}

void FolderWatcherPrivate::slotAddFolderRecursive(const QString &path)
{
    if (_pathToWatch.contains(path)) {
        return;
    }

    int subdirCount = 0;
    qCDebug(lcFolderWatcher) << "(+) Watcher:" << path;

    QDir inPath(path);
    inotifyRegisterPath(inPath.absolutePath());

    QStringList allSubfolders;
    if (!findFoldersBelow(QDir(path), allSubfolders)) {
        qCWarning(lcFolderWatcher).nospace() << "Could not traverse all sub folders of '"
                                             << path << "'";
    }

    for (const auto &subfolder : qAsConst(allSubfolders)) {
        QDir folder(subfolder);
        if (folder.exists() && !_pathToWatch.contains(folder.absolutePath())) {
            ++subdirCount;
            if (_parent->pathIsIgnored(subfolder)) {
                qCDebug(lcFolderWatcher) << "* Not adding" << folder.path();
                continue;
            }
            inotifyRegisterPath(folder.absolutePath());
        } else {
            qCDebug(lcFolderWatcher) << "    `-> discarded:" << folder.path();
        }
    }

    if (subdirCount > 0) {
        qCDebug(lcFolderWatcher) << "    `-> and" << subdirCount << "subdirectories";
    }

    qCDebug(lcFolderWatcher) << "    --- Finished scanning" << path;
}

void FolderWatcherPrivate::slotReceivedNotification(int fd)
{
    int len;
    QVarLengthArray<char, 2048> buffer(2048);

    while (true) {
        len = read(fd, buffer.data(), buffer.size());
        auto error = errno;
        /**
          * From inotify documentation:
          *
          * The behavior when the buffer given to read(2) is too
          * small to return information about the next event
          * depends on the kernel version: in kernels  before 2.6.21,
          * read(2) returns 0; since kernel 2.6.21, read(2) fails with
          * the error EINVAL.
          */
        if (len < 0 && error == EINVAL) {
            // double the buffer size
            buffer.resize(buffer.size() * 2);
            /* and try again ... */
        } else {
            // successful read
            break;
        }
    }

    QSet<QString> paths;
    // iterate over events in buffer
    struct inotify_event *event = nullptr;
    for (size_t bytePosition = 0; // start at the beginning of the buffer
         bytePosition + sizeof(inotify_event) < static_cast<unsigned>(len); // check that we still have at least sizeof(inotify_event) left in the buffer
         bytePosition += sizeof(inotify_event) + (event ? event->len : 0)) { // skip over the header and event-payload

        // cast into an inotify_event
        event = reinterpret_cast<struct inotify_event *>(&buffer[bytePosition]);

        if (event == nullptr) {
            qCDebug(lcFolderWatcher) << "NULL event";
            continue;
        }

        if (event->len == 0 || event->wd <= -1) {
            continue;
        }

        const QByteArray fileName(event->name);

        // Filter out journal changes - redundant with filtering in FolderWatcher::pathIsIgnored.
        if (fileName.startsWith("._sync_")
            || fileName.startsWith(".csync_journal.db")
            || fileName.startsWith(".sync_")) {
            continue;
        }

        const QString p = _watchToPath[event->wd] + QLatin1Char('/') + QString::fromUtf8(fileName);
        paths.insert(p);

        if ((event->mask & (IN_MOVED_TO | IN_CREATE))
            && QFileInfo(p).isDir()
            && !_parent->pathIsIgnored(p)) {
            slotAddFolderRecursive(p);
        }
        if (event->mask & (IN_MOVED_FROM | IN_DELETE)) {
            removeFoldersBelow(p);
        }
    }
    if (!paths.isEmpty()) {
        _parent->changeDetected(paths);
    }
}

void FolderWatcherPrivate::removeFoldersBelow(const QString &path)
{
    auto it = _pathToWatch.find(path);
    if (it == _pathToWatch.end())
        return;

    const QString pathSlash = path + QLatin1Char('/');

    // Remove the entry and all subentries
    while (it != _pathToWatch.end()) {
        auto itPath = it.key();
        if (!itPath.startsWith(path))
            break;
        if (itPath != path && !itPath.startsWith(pathSlash)) {
            // order is 'foo', 'foo bar', 'foo/bar'
            ++it;
            continue;
        }

        auto wid = it.value();
        inotify_rm_watch(_fd, wid);
        _watchToPath.remove(wid);
        it = _pathToWatch.erase(it);
        qCDebug(lcFolderWatcher) << "Removed watch for" << itPath;
    }
}

} // ns mirall
