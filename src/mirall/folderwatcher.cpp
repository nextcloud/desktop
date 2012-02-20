/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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
#include <sys/inotify.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#include "mirall/inotify.h"
#include "mirall/folderwatcher.h"
#include "mirall/fileutils.h"

static const uint32_t standard_event_mask =
    IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR | IN_DONT_FOLLOW;

/* minimum amount of seconds between two
   events  to consider it a new event */
#define DEFAULT_EVENT_INTERVAL_MSEC 1000

namespace Mirall {

FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent),
      _eventsEnabled(true),
      _eventInterval(DEFAULT_EVENT_INTERVAL_MSEC),
      _root(root),
      _processTimer(new QTimer(this)),
      _lastMask(0),
      _initialSyncDone(false)
{
    // this is not the best place for this
    addIgnore("/**/.unison*");
    addIgnore("*csync_timediff.ctmp*");

    _processTimer->setSingleShot(true);
    QObject::connect(_processTimer, SIGNAL(timeout()), this, SLOT(slotProcessTimerTimeout()));

    _inotify = new INotify(standard_event_mask);
    slotAddFolderRecursive(root);
    QObject::connect(_inotify, SIGNAL(notifyEvent(int, int, const QString &)),
                     SLOT(slotINotifyEvent(int, int, const QString &)));
    // do a first synchronization to get changes while
    // the application was not running
    setProcessTimer();
}

FolderWatcher::~FolderWatcher()
{

}

QString FolderWatcher::root() const
{
    return _root;
}

void FolderWatcher::addIgnore(const QString &pattern)
{
    _ignores.append(pattern);
}

bool FolderWatcher::eventsEnabled() const
{
    return _eventsEnabled;
}

void FolderWatcher::setEventsEnabled(bool enabled)
{
    qDebug() << "    * event notification " << (enabled ? "enabled" : "disabled");
    _eventsEnabled = enabled;
    if (_eventsEnabled) {
        // schedule a queue cleanup for accumulated events
        if ( _pendingPathes.empty() )
            return;
        setProcessTimer();
    }
    else
    {
        // if we are disabling events, clear any ongoing timer
        if (_processTimer->isActive())
            _processTimer->stop();
    }
}

void FolderWatcher::clearPendingEvents()
{
    if (_processTimer->isActive())
        _processTimer->stop();
    _pendingPathes.clear();
}

int FolderWatcher::eventInterval() const
{
    return _eventInterval;
}

void FolderWatcher::setEventInterval(int seconds)
{
    _eventInterval = seconds;
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
    //qDebug() << "currently watching " << watchedFolders;
    QStringListIterator subfoldersIt(FileUtils::subFoldersList(path, FileUtils::SubFolderRecursive));
    while (subfoldersIt.hasNext()) {
        QDir folder (subfoldersIt.next());
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            subdirs++;
            //qDebug() << "(+) Watcher:" << folder.path();
            // check that it does not match the ignore list
            foreach (QString pattern, _ignores) {
                QRegExp regexp(pattern);
                regexp.setPatternSyntax(QRegExp::Wildcard);
                if (regexp.exactMatch(folder.path())) {
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

    qDebug() << "***************** Inotify Event " << mask << " on " << path;
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
        if (_inotify->directories().contains(path) &&
            QFileInfo(path).isDir());
            qDebug() << "(-) Watcher:" << path;
        _inotify->removePath(path);
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

    foreach (QString pattern, _ignores) {
        QRegExp regexp(pattern);
        regexp.setPatternSyntax(QRegExp::Wildcard);
        if (regexp.exactMatch(path)) {
            qDebug() << "* Discarded" << path;
            return;
        }
    }

    if( !_pendingPathes.contains( path )) {
        _pendingPathes[path] = 0;
    }
    _pendingPathes[path] = _pendingPathes[path]+mask;
#if 0
    _pendingPaths[path]
    _pendingPaths.append(path);
#endif
    setProcessTimer();
}

void FolderWatcher::slotProcessTimerTimeout()
{
    qDebug() << "* Processing of event queue for" << root();

    if (!_pendingPathes.empty() || !_initialSyncDone) {
        QStringList notifyPaths = _pendingPathes.keys();
        _pendingPathes.clear();
        //qDebug() << lastEventTime << eventTime;
        qDebug() << "  * Notify" << notifyPaths.size() << "changed items for" << root();
        emit folderChanged(notifyPaths);
        _initialSyncDone = true;
    }
}

void FolderWatcher::setProcessTimer()
{
    if (!_processTimer->isActive()) {
        qDebug() << "* Pending events for" << root() << "will be processed after events stop for" << eventInterval() << "seconds (" << QTime::currentTime().addSecs(eventInterval()).toString("HH:mm:ss") << ")." << _pendingPathes.size() << "events until now )";
    }
    _processTimer->start(eventInterval());
}

}

#include "folderwatcher.moc"
