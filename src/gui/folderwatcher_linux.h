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
    ~FolderWatcherPrivate();

    void addPath(const QString &path);
    void removePath(const QString &);

protected slots:
    void slotReceivedNotification(int fd);
    void slotAddFolderRecursive(const QString &path);

    /// Remove all half-built renames. Called by timer when idle for a bit.
    void wipePotentialRenames();

protected:
    struct Rename
    {
        QString from;
        QString to;
    };

    bool findFoldersBelow(const QDir &dir, QStringList &fullList);
    void inotifyRegisterPath(const QString &path);

    /// Adjusts the paths in _watches when directories are renamed.
    void applyDirectoryRename(const Rename &rename);

private:
    FolderWatcher *_parent;

    QString _folder;
    QHash<int, QString> _watches;
    QScopedPointer<QSocketNotifier> _socket;
    int _fd;

    /** Maps inotify event cookie to rename data.
     *
     * For moves two independent inotify events will be seen and they
     * can be matched via the event cookie. This field stores partial
     * information as it is received. When both sides have arrived,
     * directory moves can be processed with applyDirectoryRename().
     *
     * If we don't receive both sides (if something moves away from
     * the watched folder tree, or into it from an unwatched location)
     * the _wipePotentialRenamesSoon will eventually discard the
     * incomplete data.
     *
     * These events can even be emitted by different watches if the
     * directory parent folder changed.
     */
    QHash<quint32, Rename> _potentialRenames;

    QTimer *_wipePotentialRenamesSoon;
};
}

#endif
