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

#include "folderwatcher.h"
#include "folderwatcher_qt.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace OCC {

FolderWatcherPrivate::FolderWatcherPrivate()
    :QObject(), _parent(0)
{

}

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString &path)
    : QObject(), _parent(p)

{
    _watcher.reset(new QFileSystemWatcher);

    QObject::connect(_watcher.data(), SIGNAL(directoryChanged(QString)),
                     _parent, SLOT(changeDetected(QString)) );

    QMetaObject::invokeMethod(this, "slotAddFolderRecursive", Q_ARG(QString, path));
}

// attention: result list passed by reference!
bool FolderWatcherPrivate::findFoldersBelow( const QDir& dir, QStringList& fullList )
{
    bool ok = true;
    if( !(dir.exists() && dir.isReadable()) ) {
        qDebug() << "Non existing path coming in: " << dir.absolutePath();
        ok = false;
    } else {
        QStringList nameFilter;
        nameFilter << QLatin1String("*");
        QDir::Filters filter = QDir::Dirs | QDir::NoDotAndDotDot|QDir::NoSymLinks;
        const QStringList pathes = dir.entryList(nameFilter, filter);

        QStringList::const_iterator constIterator;
        for (constIterator = pathes.constBegin(); constIterator != pathes.constEnd();
               ++constIterator) {
            const QString fullPath(dir.path()+QLatin1String("/")+(*constIterator));
            fullList.append(fullPath);
            ok = findFoldersBelow(QDir(fullPath), fullList);
        }
    }

    return ok;
}

void FolderWatcherPrivate::slotAddFolderRecursive(const QString &path)
{
    int subdirs = 0;
    qDebug() << "(+) Watcher:" << path;

    _watcher->addPath(path);
    const QStringList watchedFolders(_watcher->directories());

    QStringList allSubfolders;
    if( !findFoldersBelow(QDir(path), allSubfolders)) {
        qDebug() << "Could not traverse all sub folders";
    }
    // qDebug() << "currently watching " << watchedFolders;
    QStringListIterator subfoldersIt(allSubfolders);
    while (subfoldersIt.hasNext()) {
        QString subfolder = subfoldersIt.next();
        // qDebug() << "  (**) subfolder: " << subfolder;
        QDir folder (subfolder);
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            subdirs++;
            if( _parent->pathIsIgnored(subfolder) ) {
                qDebug() << "* Not adding" << folder.path();
                continue;
            }
            _watcher->addPath(folder.path());
        } else {
            qDebug() << "    `-> discarded:" << folder.path();
        }
    }

    if (subdirs >0) {
        qDebug() << "    `-> and" << subdirs << "subdirectories";
    }
}

void FolderWatcherPrivate::removePath(const QString &path )
{
    _watcher->removePath(path);
}


} // namespace OCC
