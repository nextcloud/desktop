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

#include <QDebug>
#include <QTimer>
#include <QUrl>

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"

#define DEFAULT_POLL_INTERVAL_SEC 15000

namespace Mirall {

Folder::Folder(const QString &alias, const QString &path, QObject *parent)
    : QObject(parent),
      _errorCount(0),
      _path(path),
      _pollTimer(new QTimer(this)),
      _alias(alias),
      _onlyOnlineEnabled(false),
      _onlyThisLANEnabled(false),
      _online(false),
      _enabled(true)
{
    qsrand(QTime::currentTime().msec());

    _pollTimer->setSingleShot(true);
    int polltime = DEFAULT_POLL_INTERVAL_SEC - 2000+ (int)( 4000.0*qrand()/(RAND_MAX+1.0));
    _pollTimer->setInterval( polltime );

    QObject::connect(_pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTimerTimeout()));
    _pollTimer->start();

#ifdef USE_WATCHER
    _watcher = new Mirall::FolderWatcher(path, this);
    QObject::connect(_watcher, SIGNAL(folderChanged(const QStringList &)),
                     SLOT(slotChanged(const QStringList &)));
#endif
    QObject::connect(this, SIGNAL(syncStarted()),
                     SLOT(slotSyncStarted()));
    QObject::connect(this, SIGNAL(syncFinished(const SyncResult &)),
                     SLOT(slotSyncFinished(const SyncResult &)));

    _online = _networkMgr.isOnline();
    QObject::connect(&_networkMgr, SIGNAL(onlineStateChanged(bool)), SLOT(slotOnlineChanged(bool)));

    _syncResult = SyncResult( SyncResult::NotYetStarted );

}

Folder::~Folder()
{
}

QString Folder::alias() const
{
    return _alias;
}

QString Folder::path() const
{
    return _path;
}

bool Folder::syncEnabled() const
{
  return _enabled;
}

void Folder::setSyncEnabled( bool doit )
{
  _enabled = doit;
#ifdef USE_WATCHER
  _watcher->setEventsEnabled( doit );
#endif
  if( doit && ! _pollTimer->isActive() ) {
      _pollTimer->start();
  }
}

bool Folder::onlyOnlineEnabled() const
{
    return _onlyOnlineEnabled;
}

void Folder::setOnlyOnlineEnabled(bool enabled)
{
    _onlyOnlineEnabled = enabled;
}

bool Folder::onlyThisLANEnabled() const
{
    return _onlyThisLANEnabled;
}

void Folder::setOnlyThisLANEnabled(bool enabled)
{
    _onlyThisLANEnabled = enabled;
}

int Folder::pollInterval() const
{
    return _pollTimer->interval();
}

void Folder::setPollInterval(int milliseconds)
{
    _pollTimer->setInterval( milliseconds );
}

int Folder::errorCount()
{
  return _errorCount;
}

void Folder::resetErrorCount()
{
  _errorCount = 0;
}

void Folder::incrementErrorCount()
{
  // if the error count gets higher than three, the interval timer
  // of the watcher is doubled.
  _errorCount++;
  if( _errorCount > 1 ) {
#ifdef USE_WATCHER
    int interval = _watcher->eventInterval();
    int newInt = 2*interval;
    qDebug() << "Set new watcher interval to " << newInt;
    _watcher->setEventInterval( newInt );
#endif
    _errorCount = 0;
  }
}

SyncResult Folder::syncResult() const
{
  return _syncResult;
}

void Folder::evaluateSync(const QStringList &pathList)
{
  if( !_enabled ) {
    qDebug() << "*" << alias() << "sync skipped, disabled!";
    return;
  }
  if (!_online && onlyOnlineEnabled()) {
    qDebug() << "*" << alias() << "sync skipped, not online";
    return;
  }

  // stop the poll timer here. Its started again in the slot of
  // sync finished.
  qDebug() << "* " << alias() << "Poll timer disabled";
  _pollTimer->stop();
  emit scheduleToSync( this );
  // startSync( pathList );
}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. Ignoring all pending events until now";
#ifdef USE_WATCHER
    _watcher->clearPendingEvents();
#endif
    evaluateSync(QStringList());
}

void Folder::slotOnlineChanged(bool online)
{
    qDebug() << "* " << alias() << "is" << (online ? "now online" : "no longer online");
    _online = online;
}

void Folder::slotChanged(const QStringList &pathList)
{
    qDebug() << "** Changed was notified on " << pathList;
    evaluateSync(pathList);
}

void Folder::slotSyncStarted()
{
    // disable events until syncing is done
#ifdef USE_WATCHER
    _watcher->setEventsEnabled(false);
#endif
    _syncResult = SyncResult( SyncResult::SyncRunning );

    emit syncStateChange();
}

void Folder::slotSyncFinished(const SyncResult &result)
{
#ifdef USE_WATCHER
    _watcher->setEventsEnabled(true);
#endif

    _syncResult = result;
    emit syncStateChange();

    // reenable the poll timer if folder is sync enabled
    if( syncEnabled() ) {
        qDebug() << "* " << alias() << "Poll timer enabled with " << _pollTimer->interval() << "milliseconds";
        _pollTimer->start();
    } else {
        qDebug() << "* Not enabling poll timer for " << alias();
        _pollTimer->stop();
    }
}

void Folder::setBackend( const QString& b )
{
  _backend = b;
}

QString Folder::backend() const
{
  return _backend;
}

} // namespace Mirall

