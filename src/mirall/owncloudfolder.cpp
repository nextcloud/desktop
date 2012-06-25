/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.org>
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

#include "mirall/owncloudfolder.h"
#include "mirall/mirallconfigfile.h"

#include <csync.h>

#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

namespace Mirall {


ownCloudFolder::ownCloudFolder(const QString &alias,
                               const QString &path,
                               const QString &secondPath,
                               QObject *parent)
    : Folder(alias, path, secondPath, parent)
    , _secondPath(secondPath)
    , _localCheckOnly( false )
    , _localFileChanges( false )
    , _csync(0)
    , _pollTimerCnt(0)
    , _csyncError(false)
    , _wipeDb(false)
    , _lastSeenFiles(0)
{
#ifdef USE_INOTIFY
    qDebug() << "****** ownCloud folder using watcher *******";
    // The folder interval is set in the folder parent class.
#else
    /* If local polling is used, the polltimer of class Folder has to fire more
     * often
     * Set a local poll time of 2000 milliseconds, which results in a 30 seconds
     * remote poll interval, defined in slotPollTimerRemoteCheck
     */

    MirallConfigFile cfgFile;

    _pollTimer->stop();
    connect( _pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTimerRemoteCheck()));
    setPollInterval( cfgFile.localPollInterval()- 500 + (int)( 1000.0*qrand()/(RAND_MAX+1.0) ) );
    _pollTimerExceed = cfgFile.pollTimerExceedFactor();

    _pollTimerCnt = _pollTimerExceed-1; // start the syncing quickly!
    _pollTimer->start();
    qDebug() << "****** ownCloud folder using local poll *******";
#endif
}

ownCloudFolder::~ownCloudFolder()
{

}

#ifndef USE_INOTIFY
void ownCloudFolder::slotPollTimerRemoteCheck()
{
    _pollTimerCnt++;
    qDebug() << "**** Poll Timer for Folder " << alias() << " increase: " << _pollTimerCnt;
}
#endif

bool ownCloudFolder::isBusy() const
{
    return ( _csync && _csync->isRunning() );
}

QString ownCloudFolder::secondPath() const
{
    QString re(_secondPath);
    MirallConfigFile cfg;
    const QString ocUrl = cfg.ownCloudUrl(QString(), true);
    // qDebug() << "**** " << ocUrl << " <-> " << re;
    if( re.startsWith( ocUrl ) ) {
        re.remove( ocUrl );
    }

    return re;
}

void ownCloudFolder::startSync()
{
    startSync( QStringList() );
}

void ownCloudFolder::startSync(const QStringList &pathList)
{

    if (_csync && _csync->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
     delete _csync;
    _errors.clear();
    _csyncError = false;
    _wipeDb = false;

    MirallConfigFile cfgFile;

    QUrl url( _secondPath );
    if( url.scheme() == QLatin1String("http") ) {
        url.setScheme( "owncloud" );
    } else {
        // connect SSL!
        url.setScheme( "ownclouds" );
    }

#ifdef USE_INOTIFY
    // if there is a watcher and no polling, ever sync is remote.
    _localCheckOnly = false;
    _syncResult.clearErrors();
#else
    _localCheckOnly = true;
    if( _pollTimerCnt >= _pollTimerExceed || _localFileChanges ) {
        _localCheckOnly = false;
        _pollTimerCnt = 0;
        _localFileChanges = false;
        _syncResult.clearErrors();
    }
#endif
    _syncResult.setLocalRunOnly( _localCheckOnly );
    Folder::startSync( pathList );


    qDebug() << "*** Start syncing url to ownCloud: " << url.toString() << ", with localOnly: " << _localCheckOnly;
    _csync = new CSyncThread( path(), url.toString(), _localCheckOnly );
    _csync->setUserPwd( cfgFile.ownCloudUser(), cfgFile.ownCloudPasswd() );
    QObject::connect(_csync, SIGNAL(started()),  SLOT(slotCSyncStarted()));
    QObject::connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()));
    QObject::connect(_csync, SIGNAL(terminated()), SLOT(slotCSyncTerminated()));
    connect(_csync, SIGNAL(csyncError(const QString)), SLOT(slotCSyncError(const QString)));
    connect(_csync, SIGNAL(csyncStateDbFile(QString)), SLOT(slotCsyncStateDbFile(QString)));
    connect(_csync, SIGNAL(wipeDb()),SLOT(slotWipeDb()));

    connect( _csync, SIGNAL(treeWalkResult(WalkStats*)),
             this, SLOT(slotThreadTreeWalkResult(WalkStats*)));
    _csync->start();
}

void ownCloudFolder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    emit syncStarted();
}

void ownCloudFolder::slotThreadTreeWalkResult( WalkStats *wStats )
{
    qDebug() << "Seen files: " << wStats->seenFiles;

    /* check if there are happend changes in the file system */
    qDebug() << "New     files: " << wStats->newFiles;
    qDebug() << "Updated files: " << wStats->eval;
    qDebug() << "Walked  files: " << wStats->seenFiles;
    qDebug() << "Eval files: "    << wStats->eval;
    qDebug() << "Removed files: " << wStats->removed;
    qDebug() << "Renamed files: " << wStats->renamed;

    if( ! _localCheckOnly ) _lastSeenFiles = 0;
    _localFileChanges = false;

#ifndef USE_INOTIFY
    if( _lastSeenFiles > 0 && _lastSeenFiles != wStats->seenFiles ) {
        qDebug() << "*** last seen files different from currently seen number " << _lastSeenFiles << "<>" << wStats->seenFiles << " => full Sync needed";
        _localFileChanges = true;
    }
    if( (wStats->newFiles + wStats->eval + wStats->removed + wStats->renamed) > 0 ) {
         qDebug() << "*** Local changes, lets do a full sync!" ;
         _localFileChanges = true;
    }
    if( _pollTimerCnt < _pollTimerExceed ) {
        qDebug() << "     *** No local changes, finalize, pollTimerCounter is "<< _pollTimerCnt ;
    }
#endif
    _lastSeenFiles = wStats->seenFiles;

    /*
     * Attention: This is deleted here, outside of the thread, because the thread can
     * faster die than this routine has read out the memory.
     */
    delete wStats->sourcePath;
    delete wStats;
}

void ownCloudFolder::slotCSyncError(const QString& err)
{
    _errors.append( err );
    _csyncError = true;
}

void ownCloudFolder::slotCsyncStateDbFile( const QString& file )
{
    qDebug() << "Got csync statedb file: " << file;
    _csyncStateDbFile = file;
}

void ownCloudFolder::slotCSyncTerminated()
{
    // do not ask csync here for reasons.
    _syncResult.setStatus( SyncResult::Error );
    _errors.append( tr("The CSync thread terminated.") );
    _syncResult.setErrorStrings(_errors);
    _csyncError = true;
    qDebug() << "-> CSync Terminated!";
    // emit syncFinished( _syncResult );
}

void ownCloudFolder::slotCSyncFinished()
{
    qDebug() << "-> CSync Finished slot with error " << _csyncError;

    if (_csyncError) {

        _syncResult.setStatus(SyncResult::Error);

        qDebug() << "  ** error Strings: " << _errors;
        _syncResult.setErrorStrings( _errors );
        qDebug() << "    * owncloud csync thread finished with error";
        if( _wipeDb ) wipe();
    } else {
        _syncResult.setStatus(SyncResult::Success);
    }

    if( ! _localCheckOnly ) _lastSeenFiles = 0;

    emit syncFinished( _syncResult );
}

void ownCloudFolder::slotTerminateSync()
{
    qDebug() << "folder " << alias() << " Terminating!";
    QString configDir = _csync->csyncConfigDir();
    qDebug() << "csync's Config Dir: " << configDir;

    if( _csync ) {
        _csync->terminate();
        _csync->wait();
        delete _csync;
        _csync = 0;
    }

    if( ! configDir.isEmpty() ) {
        QFile file( configDir + QLatin1String("/lock"));
        if( file.exists() ) {
            qDebug() << "After termination, lock file exists and gets removed.";
            file.remove();
        }
    }
}

// an error condition in csyncthread requires to get rid of the database to avoid deletion
// of files.
void ownCloudFolder::slotWipeDb()
{
    qDebug() << "Wiping of the csync database is required!";
    _wipeDb = true;
}

// This removes the csync File database if the sync folder definition is removed
// permanentely. This is needed to provide a clean startup again in case another
// local folder is synced to the same ownCloud.
// See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-788
void ownCloudFolder::wipe()
{
    if( !_csyncStateDbFile.isEmpty() ) {
        QFile file(_csyncStateDbFile);
        if( file.exists() ) {
            if( !file.remove()) {
                qDebug() << "WRN: Failed to remove existing csync StateDB " << _csyncStateDbFile;
            } else {
                qDebug() << "wipe: Removed csync StateDB " << _csyncStateDbFile;
            }
        } else {
            qDebug() << "WRN: statedb is empty, can not remove.";
        }
        // Check if the tmp database file also exists
        QString ctmpName = _csyncStateDbFile + ".ctmp";
        QFile ctmpFile( ctmpName );
        if( ctmpFile.exists() ) {
            ctmpFile.remove();
        }
        _wipeDb = false;
    }
}

} // ns

