/*
 * Copyright (C) by Markus Goetz <markus@woboq.com>
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

#ifndef MIRALL_FOLDERWATCHER_MAC_H
#define MIRALL_FOLDERWATCHER_MAC_H

#include <QObject>
#include <QString>

#include <CoreServices/CoreServices.h>


namespace OCC {

class FolderWatcher;

/**
 * @brief Mac OS X API implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate
{
public:
    FolderWatcherPrivate(FolderWatcher *p, const QString &path);
    ~FolderWatcherPrivate();

    void startWatching();
    void doNotifyParent(const QStringList &);

    /// On OSX the watcher is ready when the ctor finished.
    constexpr bool isReady() const { return true; }

private:
    FolderWatcher *_parent;

    QString _folder;

    FSEventStreamRef _stream;
};

} // namespace OCC

#endif
