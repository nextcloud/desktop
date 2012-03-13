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

#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include "csync.h"

#include "mirall/owncloudfolder.h"
#include "mirall/mirallconfigfile.h"

namespace Mirall {

ownCloudFolder::ownCloudFolder(const QString &alias,
                               const QString &path,
                               const QString &secondPath,
                               QObject *parent)
    : Folder(alias, path, parent)
    , _secondPath(secondPath)
    , _localCheckOnly( false )
    , _csync(0)
    , _pollTimerCnt(0)
    , _lastWalkedFiles(-1)

{
#ifdef USE_WATCHER
    qDebug() << "****** ownCloud folder using watcher *******";
    // The folder interval is set in the folder parent class.
#else
    /* If local polling is used, the polltimer of class Folder has to fire more
     * often
     * Set a local poll time of 2000 milliseconds, which results in a 30 seconds
     * remote poll interval, defined in slotPollTimerRemoteCheck
     */

    _pollTimer->stop();
    connect( _pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTimerRemoteCheck()));
    setPollInterval( 2000 );
    _pollTimer->start();
    qDebug() << "****** ownCloud folder using local poll *******";
#endif
}

ownCloudFolder::~ownCloudFolder()
{
}

#ifndef USE_WATCHER
void ownCloudFolder::slotPollTimerRemoteCheck()
{
    _localCheckOnly = true;
    _pollTimerCnt++;
    if( _pollTimerCnt == 15 ) {
        _pollTimerCnt = 0;
        _localCheckOnly = false;
    }
    qDebug() << "**** CSyncFolder Poll Timer check: " << _pollTimerCnt << " - " << _localCheckOnly;
}
#endif

bool ownCloudFolder::isBusy() const
{
    return false;
}

QString ownCloudFolder::secondPath() const
{
    return _secondPath;
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

    MirallConfigFile cfgFile;

    QUrl url( _secondPath );
    url.setScheme( "owncloud" );
    qDebug() << "*** Start syncing to ownCloud, onlyLocal: " << _localCheckOnly;

    _csync = new CSyncThread(path(), url.toEncoded(), _localCheckOnly );
    _csync->setUserPwd( cfgFile.ownCloudUser(), cfgFile.ownCloudPasswd() );
    QObject::connect(_csync, SIGNAL(started()),  SLOT(slotCSyncStarted()));
    QObject::connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()));
    _csync->start();
}

void ownCloudFolder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    emit syncStarted();
}

void ownCloudFolder::slotCSyncFinished()
{
    if (_csync->error())
        qDebug() << "    * owncloud csync thread finished with error";
    else
        qDebug() << "    * owncloud csync thread finished successfully";

#ifndef USE_WATCHER
    if( _csync->hasLocalChanges( _lastWalkedFiles ) ) {
        qDebug() << "Last walked files: " << _lastWalkedFiles << " against " << _csync->walkedFiles();
        qDebug() << "*** Local changes, lets do a full sync!" ;
        _localCheckOnly = false;
        _pollTimerCnt = 0;
        _lastWalkedFiles = -1;
        QTimer::singleShot( 0, this, SLOT(startSync( QStringList() )));
    } else {
        qDebug() << "     *** Finalize, pollTimerCounter is "<< _pollTimerCnt ;
        _lastWalkedFiles = _csync->walkedFiles();
    // TODO delete thread
    }
#endif
    emit syncFinished(_csync->error() ?
                          SyncResult(SyncResult::Error)
                        : SyncResult(SyncResult::Success));

}

} // ns

