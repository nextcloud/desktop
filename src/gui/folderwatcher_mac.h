/*
 * Copyright (C) by Markus Goetz <markus@woboq.com>
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

#ifndef MIRALL_FOLDERWATCHER_MAC_H
#define MIRALL_FOLDERWATCHER_MAC_H

#include <QObject>
#include <QString>

#include <CoreServices/CoreServices.h>


namespace OCC
{

class FolderWatcherPrivate
{
public:

    FolderWatcherPrivate(FolderWatcher *p, const QString &path);
    ~FolderWatcherPrivate();

    void addPath(const QString &) {}
    void removePath(const QString &) {}

    void startWatching();
    void doNotifyParent(const QStringList &);

private:
    FolderWatcher *_parent;

    QString _folder;

    FSEventStreamRef _stream;
};

}

#endif
