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

#include <QAction>
#include <QDebug>
#include <QDesktopServices>
#include <QIcon>
#include <QMutexLocker>
#include <QTimer>
#include <QUrl>

#include "mirall/constants.h"
#include "mirall/folder.h"
#include "mirall/folderwatcher.h"

#define DEFAULT_POLL_INTERVAL_SEC 30

namespace Mirall {

Folder::Folder(const QString &alias, const QString &path, QObject *parent)
    : QObject(parent),
      _path(path),
      _pollTimer(new QTimer(this)),
      _pollInterval(DEFAULT_POLL_INTERVAL_SEC),
      _alias(alias)
{
    _openAction = new QAction(QIcon(FOLDER_ICON), path, this);
    _openAction->setIconVisibleInMenu(true);
    _openAction->setIcon(QIcon(FOLDER_ICON));

    QObject::connect(_openAction, SIGNAL(triggered(bool)), SLOT(slotOpenFolder()));

    _pollTimer->setSingleShot(true);
    _pollTimer->setInterval(pollInterval() * 1000);
    QObject::connect(_pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTimerTimeout()));
    _pollTimer->start();

    _watcher = new Mirall::FolderWatcher(path, this);
    QObject::connect(_watcher, SIGNAL(folderChanged(const QStringList &)),
                     SLOT(slotChanged(const QStringList &)));

    QObject::connect(this, SIGNAL(syncStarted()),
                     SLOT(slotSyncStarted()));
    QObject::connect(this, SIGNAL(syncFinished()),
                     SLOT(slotSyncFinished()));

}

QAction * Folder::openAction() const
{
    return _openAction;
}

Folder::~Folder()
{
}

QString Folder::alias() const
{
    return _alias;
}

QString Folder::path() const
{
    return _path;
}

int Folder::pollInterval() const
{
    return _pollInterval;
}

void Folder::setPollInterval(int seconds)
{
    _pollInterval = seconds;
}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. Ignoring all pending events until now";
    _watcher->clearPendingEvents();
    qDebug() << "* " << path() << "Poll timer disabled";
    _pollTimer->stop();
    startSync(QStringList());
}

void Folder::slotChanged(const QStringList &pathList)
{
    startSync(pathList);
}

void Folder::slotOpenFolder()
{
    QDesktopServices::openUrl(QUrl(_path));
}

void Folder::slotSyncStarted()
{
    // disable events until syncing is done
    _watcher->setEventsEnabled(false);
    _openAction->setIcon(QIcon(FOLDER_SYNC_ICON));
}

void Folder::slotSyncFinished()
{
    _watcher->setEventsEnabled(true);
    _openAction->setIcon(QIcon(FOLDER_ICON));
    // reenable the poll timer
    qDebug() << "* " << path() << "Poll timer enabled";
    _pollTimer->start();
}

} // namespace Mirall

#include "folder.moc"
