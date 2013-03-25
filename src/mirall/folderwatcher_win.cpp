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

#include <QThread>
#include <QDebug>

#include "mirall/folderwatcher.h"
#include "mirall/folderwatcher_win.h"

#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

namespace Mirall {

void WatcherThread::run()
{
    _handle = FindFirstChangeNotification((wchar_t*)_path.utf16(),
                                         true, // recursive watch
                                         FILE_NOTIFY_CHANGE_FILE_NAME |
                                         FILE_NOTIFY_CHANGE_DIR_NAME |
                                         FILE_NOTIFY_CHANGE_LAST_WRITE);

    if (_handle == INVALID_HANDLE_VALUE)
    {
        qDebug() << Q_FUNC_INFO << "FindFirstChangeNotification function failed, stopping watcher!";
        FindCloseChangeNotification(_handle);
        _handle = 0;
        return;
    }

    if (_handle == NULL)
    {
        qDebug() << Q_FUNC_INFO << "FindFirstChangeNotification returned null, stopping watcher!";
        FindCloseChangeNotification(_handle);
        _handle = 0;
        return;
    }

    while(true) {
        switch(WaitForSingleObject(_handle, /*wait*/ INFINITE)) {
        case WAIT_OBJECT_0:
            if (FindNextChangeNotification(_handle) == false) {
                qDebug() << Q_FUNC_INFO << "FindFirstChangeNotification returned FALSE, stopping watcher!";
                FindCloseChangeNotification(_handle);
                _handle = 0;
                return;
            }
            // qDebug() << Q_FUNC_INFO << "Change detected in" << _path << "from" << QThread::currentThread    ();
            emit changed(_path);
            break;
        default:
            qDebug()  << Q_FUNC_INFO << "Error while watching";
        }
    }
}

WatcherThread::~WatcherThread()
{
    if (_handle)
        FindCloseChangeNotification(_handle);
}

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p)
    : _parent(p)
{
    _thread = new WatcherThread(p->root());
    connect(_thread, SIGNAL(changed(const QString&)),
            _parent,SLOT(changeDetected(const QString&)));
    _thread->start();
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
    _thread->terminate();
    _thread->wait();
    delete _thread;
}

} // namespace Mirall
