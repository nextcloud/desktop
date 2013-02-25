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
#include "config.h"

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/syncresult.h"

#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QFileSystemWatcher>
#include <QDir>

namespace Mirall {

Folder::Folder(const QString &alias, const QString &path, const QString& secondPath, QObject *parent)
    : QObject(parent),
      _errorCount(0),
      _path(path),
      _secondPath(secondPath),
      _pollTimer(new QTimer(this)),
      _alias(alias),
      _onlyOnlineEnabled(false),
      _onlyThisLANEnabled(false),
      _online(false),
      _enabled(true)
{
    qsrand(QTime::currentTime().msec());
    MirallConfigFile cfgFile;

    _pollTimer->setSingleShot(true);
    int polltime = cfgFile.remotePollInterval()- 2000 + (int)( 4000.0*qrand()/(RAND_MAX+1.0));
    qDebug() << "setting remote poll timer interval to" << polltime << "msec for folder " << alias;
    _pollTimer->setInterval( polltime );

    QObject::connect(_pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTimerTimeout()));
    _pollTimer->start();

    _watcher = new Mirall::FolderWatcher(path, this);

    MirallConfigFile cfg;

    _watcher->setIgnoreListFile( cfg.excludeFile() );

    QObject::connect(_watcher, SIGNAL(folderChanged(const QStringList &)),
                     SLOT(slotChanged(const QStringList &)));
    QObject::connect(this, SIGNAL(syncStarted()),
                     SLOT(slotSyncStarted()));
    QObject::connect(this, SIGNAL(syncFinished(const SyncResult &)),
                     SLOT(slotSyncFinished(const SyncResult &)));

#if QT_VERSION >= 0x040700
    _online = _networkMgr.isOnline();
    QObject::connect(&_networkMgr, SIGNAL(onlineStateChanged(bool)), SLOT(slotOnlineChanged(bool)));
#else
    _online = true;
#endif

    _syncResult.setStatus( SyncResult::NotYetStarted );

    // check if the local path exists
    checkLocalPath();
}

Folder::~Folder()
{
}

void Folder::checkLocalPath()
{
    QFileInfo fi(_path);

    if( fi.isDir() && fi.isReadable() ) {
        qDebug() << "Checked local path ok";
    } else {
        if( !fi.exists() ) {
            // try to create the local dir
            QDir d(_path);
            if( d.mkpath(_path) ) {
                qDebug() << "Successfully created the local dir " << _path;
            }
        }
        // Check directory again
        if( !fi.exists() ) {
            _syncResult.setErrorString(tr("Local folder %1 does not exist.").arg(_path));
            _syncResult.setStatus( SyncResult::SetupError );
        } else if( !fi.isDir() ) {
            _syncResult.setErrorString(tr("%1 should be a directory but is not.").arg(_path));
            _syncResult.setStatus( SyncResult::SetupError );
        } else if( !fi.isReadable() ) {
            _syncResult.setErrorString(tr("%1 is not readable.").arg(_path));
            _syncResult.setStatus( SyncResult::SetupError );
        }
    }

    // if all is fine, connect a FileSystemWatcher
    if( _syncResult.status() != SyncResult::SetupError ) {
        _pathWatcher = new QFileSystemWatcher(this);
        _pathWatcher->addPath( _path );
        connect(_pathWatcher, SIGNAL(directoryChanged(QString)),
                SLOT(slotLocalPathChanged(QString)));
    }
}

QString Folder::alias() const
{
    return _alias;
}

QString Folder::path() const
{
    QString p(_path);
    if( ! p.endsWith(QLatin1Char('/')) ) {
        p.append(QLatin1Char('/'));
    }
    return p;
}

QString Folder::secondPath() const
{
    return _secondPath;
}

QString Folder::nativePath() const
{
    return QDir::toNativeSeparators(_path);
}

QString Folder::nativeSecondPath() const
{
    return secondPath();
}

bool Folder::syncEnabled() const
{
  return _enabled;
}

void Folder::setSyncEnabled( bool doit )
{
  _enabled = doit;
  _watcher->setEventsEnabled( doit );
  if( doit && ! _pollTimer->isActive() ) {
      _pollTimer->start();
  }

  qDebug() << "setSyncEnabled - ############################ " << doit;
  if( doit ) {
      // undefined until next sync
      _syncResult.setStatus( SyncResult::NotYetStarted);
      _syncResult.clearErrors();
      evaluateSync( QStringList() );
  } else {
      // disable folder. Done through the _enabled-flag set above
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

void Folder::setSyncState(SyncResult::Status state)
{
    _syncResult.setStatus(state);
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
    int interval = _watcher->eventInterval();
    int newInt = 2*interval;
    qDebug() << "Set new watcher interval to " << newInt;
    _watcher->setEventInterval( newInt );
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

  _syncResult.setStatus( SyncResult::NotYetStarted );
  emit scheduleToSync( alias() );

}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. Ignoring all pending events until now";
    _watcher->clearPendingEvents();
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
    _watcher->setEventsEnabled(false);
}

void Folder::slotSyncFinished(const SyncResult &result)
{
    _watcher->setEventsEnabledDelayed(2000);

    qDebug() << "OO folder slotSyncFinished: result: " << int(result.status()) << " local: " << result.localRunOnly();
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

void Folder::slotLocalPathChanged( const QString& dir )
{
    QDir notifiedDir(dir);
    QDir localPath(_path );

    if( notifiedDir == localPath ) {
        if( !localPath.exists() ) {
            qDebug() << "ALARM: The local path was DELETED!";
        }
    }
}

void Folder::setConfigFile( const QString& file )
{
    _configFile = file;
}

QString Folder::configFile()
{
    return _configFile;
}

void Folder::setBackend( const QString& b )
{
  _backend = b;
}

QString Folder::backend() const
{
  return _backend;
}

void Folder::wipe()
{

}

} // namespace Mirall

