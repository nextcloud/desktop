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

#include <QMutexLocker>
#include <QProcess>
#include "mirall/gitfolder.h"

namespace Mirall {

    GitFolder::GitFolder(const QString &alias,
                         const QString &path,
                         const QString &remote,
                         QObject *parent)
    : Folder(alias, path, parent)
    , _remote(remote)
{
    _syncProcess = new QProcess();
}

GitFolder::~GitFolder()
{
}

void GitFolder::startSync()
{
    QMutexLocker locker(&_syncMutex);
    emit syncStarted();
    emit syncFinished();
}

} // ns

#include "gitfolder.moc"
