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
      , _csyncError(false)

{
}

CSyncFolder::~CSyncFolder()
{
}

bool CSyncFolder::isBusy() const
{
    return (_csync && _csync->isRunning() );
}

void CSyncFolder::startSync(const QStringList &pathList)
{
    if (_csync && _csync->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    delete _csync;
    _errors.clear();
    _csyncError = false;

    _csync = new CSyncThread( path(), secondPath() );
    connect(_csync, SIGNAL(started()), SLOT(slotCSyncStarted()));
    connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()));
    connect(_csync, SIGNAL(csyncError(QString)), SLOT(slotCSyncError(QString)));

    _csync->start();
}

void CSyncFolder::slotTerminateSync()
{
    if( _csync ) {
        _csync->terminate();
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
        res.setErrorString( _errors.join("\\n"));
    }
    emit syncFinished( res );
}

void CSyncFolder::slotCSyncError( const QString& errorStr )
{
    _errors.append( errorStr );
    _csyncError = true;
}

} // ns

