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
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>

#include "csync.h"

#include "mirall/csyncfolder.h"

namespace Mirall {

CSyncThread::CSyncThread(const QString &source, const QString &target)
    : _source(source)
    , _target(target)
    , _error(0)
{

}

CSyncThread::~CSyncThread()
{

}

bool CSyncThread::error() const
{
    return _error != 0;
}

QMutex CSyncThread::_mutex;

void CSyncThread::run()
{
    QMutexLocker locker(&_mutex);

    CSYNC *csync;

    _error = csync_create(&csync,
                          _source.toLocal8Bit().data(),
                          _target.toLocal8Bit().data());
    if (error())
        return;

    _error = csync_init(csync);
    if (error())
        goto cleanup;

    _error = csync_update(csync);
    if (error())
        goto cleanup;

    _error = csync_reconcile(csync);
    if (error())
        goto cleanup;

    _error = csync_propagate(csync);
cleanup:
    csync_destroy(csync);
}

CSyncFolder::CSyncFolder(const QString &alias,
                         const QString &path,
                         const QString &secondPath,
                         QObject *parent)
      : Folder(alias, path, parent)
      , _secondPath(secondPath)
      , _csync(0)
{
}

CSyncFolder::~CSyncFolder()
{
}

bool CSyncFolder::isBusy() const
{
    return false;
}

QString CSyncFolder::secondPath() const
{
    return _secondPath;
}

void CSyncFolder::startSync(const QStringList &pathList)
{
    if (_csync && _csync->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    delete _csync;

    _csync = new CSyncThread(path(), secondPath());
    QObject::connect(_csync, SIGNAL(started()), SLOT(slotCSyncStarted()));
    QObject::connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()));
    _csync->start();
}

void CSyncFolder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    emit syncStarted();
}

void CSyncFolder::slotCSyncFinished()
{
    if (_csync->error())
        qDebug() << "    * csync thread finished with error";
    else
        qDebug() << "    * csync thread finished successfully";

    // TODO delete thread
    emit syncFinished(_csync->error() ?
                      SyncResult(SyncResult::Error)
                      : SyncResult(SyncResult::Success));
}

} // ns

#include "csyncfolder.moc"
