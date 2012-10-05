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

    // Proxy settings. Proceed them as strings to csync thread.
    int intProxy = cfgFile.proxyType();

    QString proxyHost = cfgFile.proxyHostName();
    int proxyPort     = cfgFile.proxyPort();
    QString proxyUser = cfgFile.proxyUser();
    QString proxyPwd  = cfgFile.proxyPassword();

    if( intProxy == QNetworkProxy::DefaultProxy ) {
        // in case of system proxy we set the proxy in csync explicitely to the
        // value of Qt as Qt should be able to handle the pac system configuration
        // while libproxy (through libneon) might not on the target platform
        QNetworkProxy proxy = ownCloudInfo::instance()->qnamProxy();
        if( (!proxyHost.isEmpty()) && (proxyPort != 0) ) {
            intProxy  = QNetworkProxy::HttpProxy; // switch to http proxy. Tells csync/owncloud to
            // explicitely set the proxy host and port.
        }

        proxyHost = proxy.hostName();
        proxyPort = proxy.port();
        proxyUser = proxy.user();
        proxyPwd  = proxy.password();
        qDebug() << "Re-Using the Qt proxy settings for csync, host: " << proxyHost;
    }

    QString proxyType;
    if( intProxy == QNetworkProxy::NoProxy )
        proxyType = QLatin1String("NoProxy");
    else if( intProxy == QNetworkProxy::DefaultProxy )
        proxyType = QLatin1String("DefaultProxy");
    else if( intProxy == QNetworkProxy::Socks5Proxy )
        proxyType = QLatin1String("Socks5Proxy");
    else if( intProxy == QNetworkProxy::HttpProxy )
        proxyType = QLatin1String("HttpProxy");
    else if( intProxy == QNetworkProxy::HttpCachingProxy )
        proxyType = QLatin1String("HttpCachingProxy");
    else if( intProxy == QNetworkProxy::FtpCachingProxy )
        proxyType = QLatin1String("FtpCachingProxy");
    else proxyType = QLatin1String("NoProxy");

    _csync->setConnectionDetails( cfgFile.ownCloudUser(), cfgFile.ownCloudPasswd(), proxyType,
                                  proxyHost, proxyPort, proxyUser, proxyPwd );

    connect(_csync, SIGNAL(started()),  SLOT(slotCSyncStarted()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncError(const QString)), SLOT(slotCSyncError(const QString)), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncStateDbFile(QString)), SLOT(slotCsyncStateDbFile(QString)), Qt::QueuedConnection);
    connect(_csync, SIGNAL(wipeDb()),SLOT(slotWipeDb()), Qt::QueuedConnection);

    connect( _csync, SIGNAL(treeWalkResult(WalkStats*)),
             this, SLOT(slotThreadTreeWalkResult(WalkStats*)), Qt::QueuedConnection);
    _thread->start();
    QMetaObject::invokeMethod(_csync, "startSync", Qt::QueuedConnection);

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
    if(wStats->sourcePath) delete[] wStats->sourcePath;
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
        delete _csync;
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
                slotWipeDb();
            } else {
                qDebug() << "CSync not running, wipe it now!!";
                wipe();
            }

            qDebug() << "ALARM: The local path was DELETED!";
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
        QString ctmpName = _csyncStateDbFile + QLatin1String(".ctmp");
        QFile ctmpFile( ctmpName );
        if( ctmpFile.exists() ) {
            ctmpFile.remove();
        }
        _wipeDb = false;
    }
}

} // ns

