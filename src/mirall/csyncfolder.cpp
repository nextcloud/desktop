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
{

}

CSyncThread::~CSyncThread()
{

}

void CSyncThread::run()
{
    CSYNC *csync;

    if ( csync_create(&csync,
                      _source.toLocal8Bit().data(),
                      _target.toLocal8Bit().data()) != 0) {
        // handle error
        qCritical() << "csync_create error";
        return;
    }

    if (csync_init(csync) != 0) {
        qCritical() << "csync_init error";
        return;
    }

    if (csync_update(csync) != 0) {
        qCritical() << "csync_update error";
        return;
    }

    if (csync_reconcile(csync) != 0) {
        qCritical() << "csync_reconcile error";
        return;
    }

    if (csync_propagate(csync) != 0) {
        qCritical() << "csync_propagate error";
        return;
    }

    if (csync_destroy(csync) != 0) {
        qCritical() << "csync_destroy error";
        return;
    }
}

CSyncFolder::CSyncFolder(const QString &alias,
                         const QString &path,
                         const QString &secondPath,
                         QObject *parent)
      : Folder(alias, path, parent)
      , _secondPath(secondPath)
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
    //QMutexLocker locker(&_syncMutex);
    CSyncThread *csync = new CSyncThread(path(), secondPath());
    QObject::connect(csync, SIGNAL(started()), SLOT(slotCSyncStarted()));
    QObject::connect(csync, SIGNAL(finished()), SLOT(slotCSyncFinished()));
    csync->start();
}

void CSyncFolder::slotCSyncStarted()
{
    qDebug() << "* csync thread started";
    emit syncStarted();
}

void CSyncFolder::slotCSyncFinished()
{
    qDebug() << "* csync thread finished";
    // TODO delete thread
    emit syncFinished();
}

} // ns

#include "csyncfolder.moc"
