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

#include "csync.h"

#include "mirall/owncloudfolder.h"

namespace Mirall {

ownCloudFolder::ownCloudFolder(const QString &alias,
                               const QString &path,
                               const QString &secondPath,
                               QObject *parent)
    : Folder(alias, path, parent)
    , _secondPath(secondPath)
    , _csync(0)
{

}

ownCloudFolder::~ownCloudFolder()
{
}

bool ownCloudFolder::isBusy() const
{
    return false;
}

QString ownCloudFolder::secondPath() const
{
    return _secondPath;
}

void ownCloudFolder::startSync(const QStringList &pathList)
{
    if (_csync && _csync->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    delete _csync;

    /* Fix the url and remove user and password */
    QUrl url( _secondPath );
    url.setScheme( "owncloud" );
    qDebug() << "*** Start syncing to ownCloud";

    _csync = new CSyncThread(path(), url.toEncoded() );
    QObject::connect(_csync, SIGNAL(started()), SLOT(slotCSyncStarted()));
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
        qDebug() << "    * csync thread finished with error";
    else
        qDebug() << "    * csync thread finished successfully";

    // TODO delete thread
    emit syncFinished(_csync->error() ?
                      SyncResult(SyncResult::Error)
                      : SyncResult(SyncResult::Success));
}

} // ns

#include "owncloudfolder.moc"
