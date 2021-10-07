/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef MIRALL_FOLDERWATCHER_LINUX_H
#define MIRALL_FOLDERWATCHER_LINUX_H

#include <QObject>
#include <QString>
#include <QSocketNotifier>
#include <QHash>
#include <QDir>

#include "folderwatcher.h"

class QTimer;

namespace OCC {

/**
 * @brief Linux (inotify) API implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate : public QObject
{
    Q_OBJECT
public:
    FolderWatcherPrivate() {}
    FolderWatcherPrivate(FolderWatcher *p, const QString &path);

    int testWatchCount() const { return _pathToWatch.size(); }

    /// On linux the watcher is ready when the ctor finished.
    constexpr bool isReady() const { return true; }

protected slots:
    void slotReceivedNotification(int fd);
    void slotAddFolderRecursive(const QString &path);

protected:
    bool findFoldersBelow(const QDir &dir, QStringList &fullList);
    void inotifyRegisterPath(const QString &path);
    void removeFoldersBelow(const QString &path);

private:
    FolderWatcher *_parent;

    QString _folder;
    QHash<int, QString> _watchToPath;
    QMap<QString, int> _pathToWatch;
    QScopedPointer<QSocketNotifier> _socket;
    int _fd;
};
}

#endif
