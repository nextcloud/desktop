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
#include "mirall/owncloudinfo.h"

#include <csync.h>

#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>

namespace Mirall {


ownCloudFolder::ownCloudFolder(const QString &alias,
                               const QString &path,
                               const QString &secondPath,
                               QObject *parent)
    : Folder(alias, path, secondPath, parent)
    , _secondPath(secondPath)
    , _thread(0)
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

/* Only used if INotify is not available. */
void ownCloudFolder::slotPollTimerRemoteCheck()
{
    _pollTimerCnt++;
    qDebug() << "**** Poll Timer for Folder " << alias() << " increase: " << _pollTimerCnt;
}

bool ownCloudFolder::isBusy() const
{
    return ( _thread && _thread->isRunning() );
}

QString ownCloudFolder::secondPath() const
{
    QString re(_secondPath);
    MirallConfigFile cfg;
    const QString ocUrl = cfg.ownCloudUrl(QString::null, true);
    // qDebug() << "**** " << ocUrl << " <-> " << re;
    if( re.startsWith( ocUrl ) ) {
        re.remove( ocUrl );
    }

    return re;
}

QString ownCloudFolder::nativeSecondPath() const
{
    // TODO: fold into secondPath() after 1.1.0 release
    QString path = secondPath();
    if (!path.startsWith(QLatin1Char('/')) || path.isEmpty())
        path.prepend(QLatin1Char('/'));
    return path;
}

void ownCloudFolder::startSync()
{
    startSync( QStringList() );
}

void ownCloudFolder::startSync(const QStringList &pathList)
{
    if (_thread && _thread->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    delete _csync;
    delete _thread;
    _errors.clear();
    _csyncError = false;
    _wipeDb = false;

    MirallConfigFile cfgFile;

    QUrl url( _secondPath );
    if( url.scheme() == QLatin1String("http") ) {
        url.setScheme( QLatin1String("owncloud") );
    } else {
        // connect SSL!
        url.setScheme( QLatin1String("ownclouds") );
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

    _thread = new QThread(this);
    _csync = new CSyncThread( path(), url.toString(), _localCheckOnly );
    _csync->moveToThread(_thread);

    QList<QNetworkProxy> proxies = QNetworkProxyFactory::proxyForQuery(QUrl(cfgFile.ownCloudUrl()));
    // We set at least one in Application
    Q_ASSERT(proxies.count() > 0);
    QNetworkProxy proxy = proxies.first();

    _csync->setConnectionDetails( cfgFile.ownCloudUser(), cfgFile.ownCloudPasswd(), proxy );

    connect(_csync, SIGNAL(started()),  SLOT(slotCSyncStarted()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncError(const QString)), SLOT(slotCSyncError(const QString)), Qt::QueuedConnection);

    qRegisterMetaType<SyncFileItemVector>("SyncFileItemVector");
    qRegisterMetaType<WalkStats>("WalkStats");
    connect( _csync, SIGNAL(treeWalkResult(SyncFileItemVector,WalkStats)),
             this, SLOT(slotThreadTreeWalkResult(SyncFileItemVector, WalkStats)), Qt::QueuedConnection);
    _thread->start();
    QMetaObject::invokeMethod(_csync, "startSync", Qt::QueuedConnection);

}

void ownCloudFolder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    emit syncStarted();
}

void ownCloudFolder::slotThreadTreeWalkResult(const SyncFileItemVector& items, const WalkStats& wStats )
{
    _items = items;
    qDebug() << "Seen files: " << wStats.seenFiles;

    /* check if there are happend changes in the file system */
    qDebug() << "New     files: " << wStats.newFiles;
    qDebug() << "Updated files: " << wStats.eval;
    qDebug() << "Walked  files: " << wStats.seenFiles;
    qDebug() << "Eval files: "    << wStats.eval;
    qDebug() << "Removed files: " << wStats.removed;
    qDebug() << "Renamed files: " << wStats.renamed;

    if( ! _localCheckOnly ) _lastSeenFiles = 0;
    _localFileChanges = false;

#ifndef USE_INOTIFY
    if( _lastSeenFiles > 0 && _lastSeenFiles != wStats.seenFiles ) {
        qDebug() << "*** last seen files different from currently seen number " << _lastSeenFiles << "<>" << wStats.seenFiles << " => full Sync needed";
        _localFileChanges = true;
    }
    if( (wStats.newFiles + wStats.eval + wStats.removed + wStats.renamed) > 0 ) {
         qDebug() << "*** Local changes, lets do a full sync!" ;
         _localFileChanges = true;
    }
    if( _pollTimerCnt < _pollTimerExceed ) {
        qDebug() << "     *** No local changes, finalize, pollTimerCounter is "<< _pollTimerCnt ;
    }
#endif
    _lastSeenFiles = wStats.seenFiles;

}

void ownCloudFolder::slotCSyncError(const QString& err)
{
    _errors.append( err );
    _csyncError = true;
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
    if( _thread && _thread->isRunning() ) {
        _thread->quit();
    }
    emit syncFinished( _syncResult );
}

void ownCloudFolder::slotTerminateSync()
{
    qDebug() << "folder " << alias() << " Terminating!";
    QString configDir = _csync->csyncConfigDir();
    qDebug() << "csync's Config Dir: " << configDir;

    if( _thread ) {
        _thread->terminate();
        _thread->wait();
// TODO: crashes on win, leak for now, fix properly after 1.1.0
#ifndef Q_OS_WIN
        delete _csync;
#endif
        delete _thread;
        _csync = 0;
        _thread = 0;
    }

    if( ! configDir.isEmpty() ) {
        QFile file( configDir + QLatin1String("/lock"));
        if( file.exists() ) {
            qDebug() << "After termination, lock file exists and gets removed.";
            file.remove();
        }
    }

    _errors.append( tr("The CSync thread terminated.") );
    _csyncError = true;
    qDebug() << "-> CSync Terminated!";
    slotCSyncFinished();
}

void ownCloudFolder::slotLocalPathChanged( const QString& dir )
{
    QDir notifiedDir(dir);
    QDir localPath( path() );

    if( notifiedDir.absolutePath() == localPath.absolutePath() ) {
        if( !localPath.exists() ) {
            qDebug() << "XXXXXXX The sync folder root was removed!!";
            if( _thread && _thread->isRunning() ) {
                qDebug() << "CSync currently running, set wipe flag!!";
            } else {
                qDebug() << "CSync not running, wipe it now!!";
                wipe();
            }

            qDebug() << "ALARM: The local path was DELETED!";
        }
    }
}

// This removes the csync File database if the sync folder definition is removed
// permanentely. This is needed to provide a clean startup again in case another
// local folder is synced to the same ownCloud.
// See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-788
void ownCloudFolder::wipe()
{
    QString stateDbFile = path()+QLatin1String(".csync_journal.db");

    QFile file(stateDbFile);
    if( file.exists() ) {
        if( !file.remove()) {
            qDebug() << "WRN: Failed to remove existing csync StateDB " << stateDbFile;
        } else {
            qDebug() << "wipe: Removed csync StateDB " << stateDbFile;
        }
    } else {
        qDebug() << "WRN: statedb is empty, can not remove.";
    }
    // Check if the tmp database file also exists
    QString ctmpName = path() + QLatin1String(".csync_journal.db.ctmp");
    QFile ctmpFile( ctmpName );
    if( ctmpFile.exists() ) {
        ctmpFile.remove();
    }
    _wipeDb = false;
}

SyncFileStatus ownCloudFolder::fileStatus( const QString& file )
{
    if( file.isEmpty() ) return STATUS_NONE;
    QFileInfo fi( path(), file );

    foreach( const SyncFileItem item, _items ) {
        qDebug() << "FileStatus compare: " << item.file << " <> " << fi.absoluteFilePath();

        if( item.file == fi.absoluteFilePath() ) {
            switch( item.instruction ) {
            case   CSYNC_INSTRUCTION_NONE:
                return STATUS_NONE;
                break;
            case   CSYNC_INSTRUCTION_EVAL:
                return STATUS_EVAL;
                break;
            case   CSYNC_INSTRUCTION_RENAME:
                return STATUS_RENAME;
                break;
            case   CSYNC_INSTRUCTION_NEW:
                return STATUS_NEW;
                break;
            case   CSYNC_INSTRUCTION_CONFLICT:
                return STATUS_CONFLICT;
                break;
            case   CSYNC_INSTRUCTION_IGNORE:
                return STATUS_IGNORE;
                break;
            case   CSYNC_INSTRUCTION_SYNC:
            case   CSYNC_INSTRUCTION_UPDATED:
                return STATUS_SYNC;
                break;
            case   CSYNC_INSTRUCTION_STAT_ERROR:
                return STATUS_STAT_ERROR;
                break;
            case   CSYNC_INSTRUCTION_ERROR:
                return STATUS_ERROR;
                break;
            case   CSYNC_INSTRUCTION_DELETED:
            case   CSYNC_INSTRUCTION_REMOVE:
                return STATUS_REMOVE;
                break;
            default:
                break;
            }
        }
    }
    return STATUS_NEW;
}

} // ns

