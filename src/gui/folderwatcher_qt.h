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

#ifndef MIRALL_FOLDERWATCHER_QT_H
#define MIRALL_FOLDERWATCHER_QT_H

#include <QObject>
#include <QDir>
#include <QFileSystemWatcher>

namespace OCC {

class FolderWatcher;

/**
 * @brief Qt API implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate : public QObject {
    Q_OBJECT
public:
    FolderWatcherPrivate();
    FolderWatcherPrivate(FolderWatcher *p, const QString& path);
    void addPath(const QString &path) { slotAddFolderRecursive(path);  }
    void removePath(const QString &);

signals:
    void error(const QString& error);

private slots:
    void slotAddFolderRecursive(const QString &path);

protected:
    bool findFoldersBelow( const QDir& dir, QStringList& fullList );

private:
    QScopedPointer<QFileSystemWatcher> _watcher;

    FolderWatcher *_parent;

};

}

#endif // MIRALL_FOLDERWATCHER_QT_H
