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
#include "mirall/folderwatcher.h"
#include "mirall/folder.h"

#include <stdint.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#if defined(Q_OS_WIN)
#include "mirall/folderwatcher_win.h"
#elif defined(Q_OS_MAC)
#include "mirall/folderwatcher_mac.h"
#elif defined(Q_OS_LINUX)
#include "mirall/folderwatcher_linux.h"
#endif


namespace Mirall {

FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent)
{
    _d.reset(new FolderWatcherPrivate(this, root));

    _timer.start();
}

FolderWatcher::~FolderWatcher()
{ }

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

bool FolderWatcher::pathIsIgnored( const QString& path )
{
    if( path.isEmpty() ) return true;

    // Remember: here only directories are checked!
    // If that changes to files too at some day, remember to check
    // for the database name as well as the trailing slash rule for
    // dirs only. Best use csync_ignore than somehow.
    foreach (QString pattern, _ignores) {
        QRegExp regexp(pattern);
        regexp.setPatternSyntax(QRegExp::Wildcard);

        QFileInfo fInfo(path);
        if( fInfo.isHidden() ) {
            qDebug() << "* Discarded as is hidden!";
            return true;
        }

        if(pattern.endsWith('/')) {
            // directory only pattern. But since only dirs here, we cut off the trailing dir.
            pattern.remove(pattern.length()-1, 1); // remove the last char.
        }
        // if the pattern contains / it needs to match the entire path
        if (pattern.contains('/') && regexp.exactMatch(path)) {
            qDebug() << "* Discarded by ignore pattern: " << path;
            return true;
        }

        QStringList components = path.split('/');
        foreach (const QString& comp, components) {
            if(regexp.exactMatch(comp)) {
                qDebug() << "* Discarded by component ignore pattern " << comp;
                return true;
            }
        }
    }
    return false;
}

void FolderWatcher::changeDetected( const QString& path )
{
    qDebug() << Q_FUNC_INFO << path;
    // Check if the same path was reported within the last second.
    if( path == _lastPath && _timer.elapsed() < 1000 ) {
        // the same path was reported within the last second. Skip.
        return;
    }
    _lastPath = path;
    _timer.restart();

    // ------- handle ignores:
    if( pathIsIgnored(path) ) {
        return;
    }

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

