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

// event masks
#include "folderwatcher.h"

#include <stdint.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#if defined(Q_OS_WIN)
#include "folderwatcher_win.h"
#elif defined(Q_OS_MAC)
#include "folderwatcher_mac.h"
#elif defined(Q_OS_UNIX)
#include "folderwatcher_linux.h"
#endif

#include "excludedfiles.h"
#include "folder.h"

namespace OCC {

FolderWatcher::FolderWatcher(const QString &root, Folder* folder)
    : QObject(folder),
      _folder(folder)
{
    _canonicalFolderPath = QFileInfo(root).canonicalFilePath();

    _d.reset(new FolderWatcherPrivate(this, _canonicalFolderPath));

    _timer.start();
}

FolderWatcher::~FolderWatcher()
{ }

bool FolderWatcher::pathIsIgnored( const QString& path )
{
    if( path.isEmpty() ) return true;
    if( !_folder ) return false;

#ifndef OWNCLOUD_TEST
    QString relPath = path;
    if (relPath.startsWith(_canonicalFolderPath)) {
        relPath = relPath.remove(0, _canonicalFolderPath.length()+1);
        if (_folder->isFileExcludedRelative(relPath)) {
            qDebug() << "* Ignoring file" << relPath << "in" << _canonicalFolderPath;
            return true;
        }
    }
    // there could be an odd watch event not being inside the _canonicalFolderPath
    // We will just not ignore it then, who knows.

#endif
    return false;
}

void FolderWatcher::changeDetected( const QString& path )
{
    QStringList paths(path);
    changeDetected(paths);
}

void FolderWatcher::changeDetected( const QStringList& paths )
{
    // qDebug() << Q_FUNC_INFO << paths;

    // TODO: this shortcut doesn't look very reliable:
    //   - why is the timeout only 1 second?
    //   - what if there is more than one file being updated frequently?
    //   - why do we skip the file altogether instead of e.g. reducing the upload frequency?

    // Check if the same path was reported within the last second.
    QSet<QString> pathsSet = paths.toSet();
    if( pathsSet == _lastPaths && _timer.elapsed() < 1000 ) {
        // the same path was reported within the last second. Skip.
        return;
    }
    _lastPaths = pathsSet;
    _timer.restart();

    QSet<QString> changedPaths;

    // ------- handle ignores:
    for (int i = 0; i < paths.size(); ++i) {
        QString path = paths[i];
        if( pathIsIgnored(path) ) {
            continue;
        }

        changedPaths.insert(path);
    }
    if (changedPaths.isEmpty()) {
        return;
    }

    qDebug() << "detected changes in paths:" << changedPaths;
    foreach (const QString &path, changedPaths) {
        emit pathChanged(path);
    }
}

void FolderWatcher::addPath(const QString &path )
{
    _d->addPath(path);
}

void FolderWatcher::removePath(const QString &path )
{
    _d->removePath(path);
}


} // namespace OCC

