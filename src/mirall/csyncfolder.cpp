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

#include "mirall/csyncfolder.h"
#include "mirall/csyncthread.h"
#include "mirall/mirallconfigfile.h"

#include <csync.h>

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>

namespace Mirall {

CSyncFolder::CSyncFolder(const QString &alias,
                         const QString &path,
                         const QString &secondPath,
                         QObject *parent)
      : Folder(alias, path, secondPath, parent)
      , _csync(0)
      , _thread(0)
      , _csyncError(false)

{
}

CSyncFolder::~CSyncFolder()
{
}

bool CSyncFolder::isBusy() const
{
    return (_csync && _thread && _thread->isRunning() );
}

void CSyncFolder::startSync(const QStringList &pathList)
{
    if (_thread && _thread->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    delete _csync;
    delete _thread;
    _errors.clear();
    _csyncError = false;
    _syncResult.setStatus( SyncResult::SyncRunning );
    emit syncStateChange();

    _thread = new QThread(this);
    // _csync = new CSyncThread( _csync );
    connect(_csync, SIGNAL(started()), SLOT(slotCSyncStarted()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncError(QString)), SLOT(slotCSyncError(QString)), Qt::QueuedConnection);
    _csync->moveToThread(_thread);
    _thread->start();
    QMetaObject::invokeMethod(_csync, "startSync", Qt::QueuedConnection);
}

void CSyncFolder::slotTerminateSync()
{
    if( _thread ) {
        _thread->terminate();
    }
}

void CSyncFolder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    emit syncStarted();
}

void CSyncFolder::slotCSyncFinished()
{
    SyncResult res(SyncResult::Success);
    if( _csyncError ) {
        res.setStatus( SyncResult::Error );
        res.setErrorString( _errors.join(QLatin1String("\n")));
    }
    emit syncFinished( res );
}

void CSyncFolder::slotCSyncError( const QString& errorStr )
{
    _errors.append( errorStr );
    _csyncError = true;
}

} // ns

