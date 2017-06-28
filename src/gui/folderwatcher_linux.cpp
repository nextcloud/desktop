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

#include "config.h"

#include <sys/inotify.h>

#include "folder.h"
#include "folderwatcher_linux.h"

#include <cerrno>
#include <QStringList>
#include <QObject>
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
        connect(_socket.data(), SIGNAL(activated(int)), SLOT(slotReceivedNotification(int)));
    } else {
        qCWarning(lcFolderWatcher) << "notify_init() failed: " << strerror(errno);
    }

    QMetaObject::invokeMethod(this, "slotAddFolderRecursive", Q_ARG(QString, path));
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
}

// attention: result list passed by reference!
bool FolderWatcherPrivate::findFoldersBelow(const QDir &dir, QStringList &fullList)
{
    bool ok = true;
    if (!(dir.exists() && dir.isReadable())) {
        qCDebug(lcFolderWatcher) << "Non existing path coming in: " << dir.absolutePath();
        ok = false;
    } else {
        QStringList nameFilter;
        nameFilter << QLatin1String("*");
        QDir::Filters filter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden;
        const QStringList pathes = dir.entryList(nameFilter, filter);

        QStringList::const_iterator constIterator;
        for (constIterator = pathes.constBegin(); constIterator != pathes.constEnd();
             ++constIterator) {
            const QString fullPath(dir.path() + QLatin1String("/") + (*constIterator));
            fullList.append(fullPath);
            ok = findFoldersBelow(QDir(fullPath), fullList);
        }
    }

    return ok;
}

void FolderWatcherPrivate::inotifyRegisterPath(const QString &path)
{
    if (!path.isEmpty()) {
        int wd = inotify_add_watch(_fd, path.toUtf8().constData(),
            IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR);
        if (wd > -1) {
            _watches.insert(wd, path);
        }
    }
}

void FolderWatcherPrivate::slotAddFolderRecursive(const QString &path)
{
    int subdirs = 0;
    qCDebug(lcFolderWatcher) << "(+) Watcher:" << path;

    QDir inPath(path);
    inotifyRegisterPath(inPath.absolutePath());

    const QStringList watchedFolders = _watches.values();

    QStringList allSubfolders;
    if (!findFoldersBelow(QDir(path), allSubfolders)) {
        qCWarning(lcFolderWatcher) << "Could not traverse all sub folders";
    }
    QStringListIterator subfoldersIt(allSubfolders);
    while (subfoldersIt.hasNext()) {
        QString subfolder = subfoldersIt.next();
        QDir folder(subfolder);
        if (folder.exists() && !watchedFolders.contains(folder.absolutePath())) {
            subdirs++;
            if (_parent->pathIsIgnored(subfolder)) {
                qCDebug(lcFolderWatcher) << "* Not adding" << folder.path();
                continue;
            }
            inotifyRegisterPath(folder.absolutePath());
        } else {
            qCDebug(lcFolderWatcher) << "    `-> discarded:" << folder.path();
        }
    }

    if (subdirs > 0) {
        qCDebug(lcFolderWatcher) << "    `-> and" << subdirs << "subdirectories";
    }
}

void FolderWatcherPrivate::slotReceivedNotification(int fd)
{
    int len;
    struct inotify_event *event;
    int i;
    int error;
    QVarLengthArray<char, 2048> buffer(2048);

    do {
        len = read(fd, buffer.data(), buffer.size());
        error = errno;
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
            continue;
        }
    } while (false);

    // reset counter
    i = 0;
    // while there are enough events in the buffer
    while (i + sizeof(struct inotify_event) < static_cast<unsigned int>(len)) {
        // cast an inotify_event
        event = (struct inotify_event *)&buffer[i];
        if (event == NULL) {
            qCDebug(lcFolderWatcher) << "NULL event";
            i += sizeof(struct inotify_event);
            continue;
        }

        // Fire event for the path that was changed.
        if (event->len > 0 && event->wd > -1) {
            QByteArray fileName(event->name);
            if (fileName.startsWith("._sync_")
                || fileName.startsWith(".csync_journal.db")
                || fileName.startsWith(".owncloudsync.log")
                || fileName.startsWith(".sync_")) {
            } else {
                const QString p = _watches[event->wd] + '/' + fileName;
                _parent->changeDetected(p);
            }
        }

        // increment counter
        i += sizeof(struct inotify_event) + event->len;
    }
}

void FolderWatcherPrivate::addPath(const QString &path)
{
    slotAddFolderRecursive(path);
}

void FolderWatcherPrivate::removePath(const QString &path)
{
    int wid = -1;
    // Remove the inotify watch.
    QHash<int, QString>::const_iterator i = _watches.constBegin();

    while (i != _watches.constEnd()) {
        if (i.value() == path) {
            wid = i.key();
            break;
        }
        ++i;
    }
    if (wid > -1) {
        inotify_rm_watch(_fd, wid);
        _watches.remove(wid);
    }
}

} // ns mirall
