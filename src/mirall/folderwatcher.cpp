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

// event masks
#include "mirall/folderwatcher.h"
#include "mirall/folder.h"
#include "mirall/inotify.h"

#include <stdint.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

namespace Mirall {

FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent),
      _root(root)
{
    _d.reset(new FolderWatcherPrivate(this));

    if( !_root.isEmpty() ) {
        QMetaObject::invokeMethod(_d.data(), "slotAddFolderRecursive", Q_ARG(QString, _root));
    }
     // do a first synchronization to get changes while
    // the application was not running
    // setProcessTimer(); FIXME
}

void FolderWatcher::addIgnoreListFile( const QString& file )
{
    if( file.isEmpty() ) return;

    QFile infile( file );
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!infile.atEnd()) {
        QString line = QString::fromLocal8Bit( infile.readLine() ).trimmed();
        if( !(line.startsWith( QLatin1Char('#') ) || line.isEmpty()) ) {
            _ignores.append(line);
        }
    }
}

QStringList FolderWatcher::ignores() const
{
    return _ignores;
}

void FolderWatcher::changeDetected( const QString& path )
{
    // FIXME: Handle the ignores here
    emit folderChanged(path);
}

void FolderWatcher::addPath(const QString &path )
{
    _d->addPath(path);
}

void FolderWatcher::removePath(const QString &path )
{
    _d->removePath(path);
}


} // namespace Mirall

