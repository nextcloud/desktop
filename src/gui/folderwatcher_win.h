/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_FOLDERWATCHER_WIN_H
#define MIRALL_FOLDERWATCHER_WIN_H

#include <QThread>
#include <windows.h>

namespace OCC {

class FolderWatcher;

/**
 * @brief The WatcherThread class
 * @ingroup gui
 */
class WatcherThread : public QThread {
    Q_OBJECT
public:
    WatcherThread(const QString &path) :
        QThread(), _path(path), _handle(0) {}

    ~WatcherThread();

protected:
    void run();
    void watchChanges(size_t fileNotifyBufferSize,
                      bool* increaseBufferSize);

signals:
    void changed(const QString &path);

private:
    QString _path;
    HANDLE _handle;
};

/**
 * @brief Windows implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate : public QObject {
    Q_OBJECT
public:
    FolderWatcherPrivate(FolderWatcher *p, const QString& path);
    ~FolderWatcherPrivate();

    void addPath(const QString &) {}
    void removePath(const QString &) {}

private:
    FolderWatcher *_parent;
    WatcherThread *_thread;
};

}

#endif // MIRALL_FOLDERWATCHER_WIN_H
