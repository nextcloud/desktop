/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
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

#include <sys/inotify.h>

#include "mirall/folderwatcher.h"

namespace Mirall {

static const uint32_t standard_event_mask =
    IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE |
    IN_CREATE |IN_DELETE | IN_DELETE_SELF |
    IN_MOVE_SELF |IN_UNMOUNT |IN_ONLYDIR |
    IN_DONT_FOLLOW;

void FolderWatcher::setupBackend() {
    _processTimer->setSingleShot(true);
    QObject::connect(_processTimer, SIGNAL(timeout()), this, SLOT(slotProcessTimerTimeout()));

    _inotify = new INotify(standard_event_mask);
    slotAddFolderRecursive(_root);
    QObject::connect(_inotify, SIGNAL(notifyEvent(int, int, const QString &)),
                     this, SLOT(slotINotifyEvent(int, int, const QString &)));
}

QStringList FolderWatcher::folders() const
{
    return _inotify->directories();
}

void FolderWatcher::slotAddFolderRecursive(const QString &path)
{
    int subdirs = 0;
    qDebug() << "(+) Watcher:" << path;

    _inotify->addPath(path);
    QStringList watchedFolders(_inotify->directories());
    // qDebug() << "currently watching " << watchedFolders;
    QStringListIterator subfoldersIt(FileUtils::subFoldersList(path, FileUtils::SubFolderRecursive));
    while (subfoldersIt.hasNext()) {
        QString subfolder = subfoldersIt.next();
        // qDebug() << "  (**) subfolder: " << subfolder;
        QDir folder (subfolder);
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            subdirs++;
            // check that it does not match the ignore list
            foreach ( const QString& pattern, _ignores) {
                QRegExp regexp(pattern);
                regexp.setPatternSyntax(QRegExp::Wildcard);
                if ( regexp.exactMatch(folder.path()) ) {
                    qDebug() << "* Not adding" << folder.path();
                    continue;
                }

            }
            _inotify->addPath(folder.path());
        }
        else
            qDebug() << "    `-> discarded:" << folder.path();
    }
    if (subdirs >0)
        qDebug() << "    `-> and" << subdirs << "subdirectories";
}

void FolderWatcher::slotINotifyEvent(int mask, int cookie, const QString &path)
{
    int lastMask = _lastMask;
    QString lastPath = _lastPath;

    _lastMask = mask;
    _lastPath = path;

    if( ! eventsEnabled() ) return;
    qDebug() << "** Inotify Event " << mask << " on " << path;
    // cancel close write events that come after create
    if (lastMask == IN_CREATE && mask == IN_CLOSE_WRITE
        && lastPath == path ) {
        return;
    }

    if (IN_IGNORED & mask) {
        //qDebug() << "IGNORE event";
        return;
    }

    if (IN_Q_OVERFLOW & mask) {
        //qDebug() << "OVERFLOW";
    }

    if (mask & IN_CREATE) {
        //qDebug() << cookie << " CREATE: " << path;
        if (QFileInfo(path).isDir()) {
            //setEventsEnabled(false);
            slotAddFolderRecursive(path);
            //setEventsEnabled(true);
        }
    }
    else if (mask & IN_DELETE) {
        //qDebug() << cookie << " DELETE: " << path;
        if ( QFileInfo(path).isDir() && _inotify->directories().contains(path) ) {
            qDebug() << "(-) Watcher:" << path;
            _inotify->removePath(path);
        }
    }
    else if (mask & IN_CLOSE_WRITE) {
        //qDebug() << cookie << " WRITABLE CLOSED: " << path;
    }
    else if (mask & IN_MOVE) {
        //qDebug() << cookie << " MOVE: " << path;
    }
    else {
        //qDebug() << cookie << " OTHER " << mask << " :" << path;
    }

    foreach (const QString& pattern, _ignores) {
        QRegExp regexp(pattern);
        regexp.setPatternSyntax(QRegExp::Wildcard);

        if (regexp.exactMatch(path)) {
            qDebug() << "* Discarded by ignore pattern: " << path;
            return;
        }
        QFileInfo fInfo(path);
        if( regexp.exactMatch(fInfo.fileName())) {
            qDebug() << "* Discarded by ignore pattern:" << path;
            return;
        }
        if( fInfo.isHidden() ) {
            qDebug() << "* Discarded as is hidden!";
            return;
        }
    }

    if( !_pendingPathes.contains( path )) {
        _pendingPathes[path] = 0;
    }
    _pendingPathes[path] = _pendingPathes[path]+mask;
    setProcessTimer();
}

} // namespace Mirall
