/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "mirall/folderwatcher.h"

namespace Mirall {

void FolderWatcher::setupBackend()
{
    // blank
}

QStringList FolderWatcher::folders() const
{
    return QStringList();
}

void FolderWatcher::slotAddFolderRecursive(const QString &path)
{
    Q_UNUSED(path);
    qDebug() << "** Watcher is not compiled in!";
}

void FolderWatcher::slotINotifyEvent(int mask, int cookie, const QString &path)
{
    // TODO: refactor!

    int lastMask = _lastMask;
    QString lastPath = _lastPath;

    _lastMask = mask;
    _lastPath = path;

    if( ! eventsEnabled() ) return;

    setProcessTimer();
}

} // namespace Mirall
