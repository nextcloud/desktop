/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include "config.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QHash>
#include <QScopedPointer>
#include <QSet>

class QTimer;

namespace OCC {

class FolderWatcherPrivate;
class Folder;

/**
 * @brief Monitors a directory recursively for changes
 *
 * Folder Watcher monitors a directory and its sub directories
 * for changes in the local file system. Changes are signalled
 * through the pathChanged() signal.
 *
 * Note that if new folders are created, this folderwatcher class
 * does not automatically add them to the list of monitored
 * dirs. That is the responsibility of the user of this class to
 * call addPath() with the new dir.
 *
 * @ingroup gui
 */

class FolderWatcher : public QObject
{
    Q_OBJECT
public:
    /**
     * @param root Path of the root of the folder
     */
    FolderWatcher(const QString &root, Folder* folder = 0L);
    virtual ~FolderWatcher();

    /**
     * Not all backends are recursive by default.
     * Those need to be notified when a directory is added or removed while the watcher is disabled.
     * This is a no-op for backends that are recursive
     */
    void addPath(const QString&);
    void removePath(const QString&);

    /* Check if the path is ignored. */
    bool pathIsIgnored( const QString& path );

signals:
    /** Emitted when one of the watched directories or one
     *  of the contained files is changed. */
    void pathChanged(const QString &path);

    /** Emitted if an error occurs */
    void error(const QString& error);

protected slots:
    // called from the implementations to indicate a change in path
    void changeDetected( const QString& path);
    void changeDetected( const QStringList& paths);

protected:
    QHash<QString, int> _pendingPathes;

private:
    QScopedPointer<FolderWatcherPrivate> _d;
    QTime _timer;
    QSet<QString> _lastPaths;
    Folder* _folder;
    QString _canonicalFolderPath;

    friend class FolderWatcherPrivate;
};

}

#endif
