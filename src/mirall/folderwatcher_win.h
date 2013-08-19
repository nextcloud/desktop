/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_FOLDERWATCHER_WIN_H
#define MIRALL_FOLDERWATCHER_WIN_H

#include <QThread>
#include <windows.h>

namespace Mirall {

class FolderWatcher;

// watcher thread

class WatcherThread : public QThread {
    Q_OBJECT
public:
    WatcherThread(const QString &path) :
        QThread(), _path(path), _handle(0) {}

    ~WatcherThread();

protected:
    void run();

signals:
    void changed(const QString &path);

private:
    QString _path;
    HANDLE _handle;
};

class FolderWatcherPrivate : public QObject {
    Q_OBJECT
public:
    FolderWatcherPrivate(FolderWatcher *p);
    ~FolderWatcherPrivate();

    void addPath(const QString &) {}
    void removePath(const QString &) {}

private:
    FolderWatcher *_parent;
    WatcherThread *_thread;
};

}

#endif // MIRALL_FOLDERWATCHER_WIN_H
