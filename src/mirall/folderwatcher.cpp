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
#include "mirall/folderwatcher.h"
#include "mirall/folder.h"
#include "mirall/inotify.h"
#include "mirall/fileutils.h"

#include <stdint.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

/* minimum amount of seconds between two
   events  to consider it a new event */
#define DEFAULT_EVENT_INTERVAL_MSEC 1000

#if defined(Q_OS_WIN)
#include "mirall/folderwatcher_win.h"
#elif defined(Q_OS_MAC)
#include "mirall/folderwatcher_mac.h"
#elif defined(USE_INOTIFY)
#include "mirall/folderwatcher_inotify.h"
#endif

namespace Mirall {

FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent),
      _eventsEnabled(true),
      _eventInterval(DEFAULT_EVENT_INTERVAL_MSEC),
      _root(root),
      _processTimer(new QTimer(this))
{
    _d = new FolderWatcherPrivate(this);

    _processTimer->setSingleShot(true);
    QObject::connect(_processTimer, SIGNAL(timeout()), this, SLOT(slotProcessTimerTimeout()));

    // do a first synchronization to get changes while
    // the application was not running
    setProcessTimer();
}

FolderWatcher::~FolderWatcher()
{
    delete _d;
}

QString FolderWatcher::root() const
{
    return _root;
}

void FolderWatcher::setIgnoreListFile( const QString& file )
{
    if( file.isEmpty() ) return;

    QFile infile( file );
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!infile.atEnd()) {
        QString line = QString::fromLocal8Bit( infile.readLine() ).trimmed();
        if( !line.startsWith( QLatin1Char('#') )) {
            addIgnore(line);
        }
    }
}

void FolderWatcher::addIgnore(const QString &pattern)
{
    if( pattern.isEmpty() ) return;
    _ignores.append(pattern);
}

QStringList FolderWatcher::ignores() const
{
    return _ignores;
}

bool FolderWatcher::eventsEnabled() const
{
    return _eventsEnabled;
}

void FolderWatcher::setEventsEnabledDelayed( int delay_msec )
{
    qDebug() << "Starting Event logging again in " << delay_msec << " milliseconds";
    QTimer::singleShot( delay_msec, this, SLOT(setEventsEnabled()));
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

void FolderWatcher::slotProcessTimerTimeout()
{
    qDebug() << "* Processing of event queue for" << root();

    if (!_pendingPathes.empty() ) {
        QStringList notifyPaths = _pendingPathes.keys();
        _pendingPathes.clear();
        //qDebug() << lastEventTime << eventTime;
        qDebug() << "  * Notify" << notifyPaths.size() << "change items for" << root();
        emit folderChanged(notifyPaths);
    }
}

void FolderWatcher::setProcessTimer()
{
    if (!_processTimer->isActive()) {
        qDebug() << "* Pending events for" << root()
                 << "will be processed after events stop for"
                 << eventInterval() << "milliseconds ("
                 << QTime::currentTime().addSecs(eventInterval()).toString(QLatin1String("HH:mm:ss"))
                 << ")." << _pendingPathes.size() << "events until now )";
    }
    _processTimer->start(eventInterval());
}

void FolderWatcher::changeDetected(const QString& f)
{
    if( ! eventsEnabled() ) {
        qDebug() << "FolderWatcher::changeDetected when eventsEnabled() -> ignore";
        return;
    }

    _pendingPathes[f] = 1; //_pendingPathes[path]+mask;
    setProcessTimer();
}

} // namespace Mirall

