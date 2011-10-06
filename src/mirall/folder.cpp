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

#define DEFAULT_POLL_INTERVAL_SEC 45

namespace Mirall {

Folder::Folder(const QString &alias, const QString &path, QObject *parent)
    : QObject(parent),
      _errorCount(0),
      _path(path),
      _pollTimer(new QTimer(this)),
      _pollInterval(DEFAULT_POLL_INTERVAL_SEC),
      _alias(alias),
      _onlyOnlineEnabled(false),
      _onlyThisLANEnabled(false),
      _online(false)
{
    _openAction = new QAction(QIcon::fromTheme(FOLDER_ICON), path, this);
    _openAction->setIconVisibleInMenu(true);
    _openAction->setIcon(QIcon::fromTheme(FOLDER_ICON));

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
    QObject::connect(this, SIGNAL(syncFinished(const SyncResult &)),
                     SLOT(slotSyncFinished(const SyncResult &)));

    _online = _networkMgr.isOnline();
    QObject::connect(&_networkMgr, SIGNAL(onlineStateChanged(bool)), SLOT(slotOnlineChanged(bool)));

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

bool Folder::onlyOnlineEnabled() const
{
    return _onlyOnlineEnabled;
}

void Folder::setOnlyOnlineEnabled(bool enabled)
{
    _onlyOnlineEnabled = enabled;
}

bool Folder::onlyThisLANEnabled() const
{
    return _onlyThisLANEnabled;
}

void Folder::setOnlyThisLANEnabled(bool enabled)
{
    _onlyThisLANEnabled = enabled;
}

int Folder::pollInterval() const
{
    return _pollInterval;
}

void Folder::setPollInterval(int seconds)
{
    _pollInterval = seconds;
}

int Folder::errorCount()
{
  return _errorCount;
}

void Folder::resetErrorCount()
{
  _errorCount = 0;
}

void Folder::incrementErrorCount()
{
  // if the error count gets higher than three, the interval timer
  // of the watcher is doubled.
  _errorCount++;
  if( _errorCount > 1 ) {
    int interval = _watcher->eventInterval();
    int newInt = 2*interval;
    qDebug() << "Set new watcher interval to " << newInt;
    _watcher->setEventInterval( newInt );
    _errorCount = 0;
  }
}

void Folder::evaluateSync(const QStringList &pathList)
{
  if (!_online && onlyOnlineEnabled()) {
    qDebug() << "*" << alias() << "sync skipped, not online";
    return;
  }
  startSync(pathList);
}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. Ignoring all pending events until now";
    _watcher->clearPendingEvents();
    qDebug() << "* " << alias() << "Poll timer disabled";
    _pollTimer->stop();
    evaluateSync(QStringList());
}

void Folder::slotOnlineChanged(bool online)
{
    qDebug() << "* " << alias() << "is" << (online ? "now online" : "no longer online");
    _online = online;
}

void Folder::slotChanged(const QStringList &pathList)
{
    evaluateSync(pathList);
}

void Folder::slotOpenFolder()
{
    QDesktopServices::openUrl(QUrl(_path));
}

void Folder::slotSyncStarted()
{
    // disable events until syncing is done
    _watcher->setEventsEnabled(false);
    _openAction->setIcon(QIcon::fromTheme(FOLDER_SYNC_ICON));
}

void Folder::slotSyncFinished(const SyncResult &result)
{
    _watcher->setEventsEnabled(true);
    _openAction->setIcon(QIcon::fromTheme(FOLDER_ICON));
    // reenable the poll timer
    qDebug() << "* " << alias() << "Poll timer enabled";
    _pollTimer->start();
}

} // namespace Mirall

#include "folder.moc"
