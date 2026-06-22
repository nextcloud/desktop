/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_FOLDERWATCHER_WIN_H
#define MIRALL_FOLDERWATCHER_WIN_H

#include "common/utility.h"
#include <QAtomicInt>
#include <QThread>
#include <windows.h>

namespace OCC {

class FolderWatcher;

/**
 * @brief The WatcherThread class
 * @ingroup gui
 */
class WatcherThread : public QThread
{
    Q_OBJECT
public:
    WatcherThread(const QString &path)
        : QThread()
        , _path(Utility::trailingSlashPath(path))
        , _directory(0)
        , _resultEvent(0)
        , _stopEvent(0)
        , _done(false)
    {
    }

    ~WatcherThread();

    void stop();

protected:
    void run();
    void watchChanges(size_t fileNotifyBufferSize,
        bool *increaseBufferSize);
    void closeHandle();

signals:
    void changed(const QString &path);
    void lostChanges();
    void ready();

private:
    QString _path;
    HANDLE _directory;
    HANDLE _resultEvent;
    HANDLE _stopEvent;
    QAtomicInt _done;
};

/**
 * @brief Windows implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate : public QObject
{
    Q_OBJECT
public:
    FolderWatcherPrivate(FolderWatcher *p, const QString &path);
    ~FolderWatcherPrivate();

    /// Set to non-zero once the WatcherThread is capturing events.
    QAtomicInt _ready;

private:
    FolderWatcher *_parent;
    WatcherThread *_thread;
};
}

#endif // MIRALL_FOLDERWATCHER_WIN_H
