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

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

namespace Mirall {

void WatcherThread::run()
{
    HANDLE handle;

    handle = FindFirstChangeNotification((wchar_t*)_path.utf16(),
                                         true, // recursive watch
                                         FILE_NOTIFY_CHANGE_FILE_NAME |
                                         FILE_NOTIFY_CHANGE_DIR_NAME |
                                         FILE_NOTIFY_CHANGE_LAST_WRITE);

    if (handle == INVALID_HANDLE_VALUE)
    {
        printf("\n ERROR: FindFirstChangeNotification function failed.\n");
        return;
    }

    if (handle == NULL)
    {
        printf("\n ERROR: FindFirstChangeNotification returned null.\n");
        return;
    }

    while(true) {
        switch(WaitForSingleObject(handle, /*wait*/ INFINITE)) {
        case WAIT_OBJECT_0:
            emit changed();
            break;
        default:
            qDebug() << "Error while watching";
        }
    }
}
// watcher thread

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p)
    : _parent(p)
{
    _thread = new WatcherThread(p->root());
    _thread->run();
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
    delete _thread;
}

} // namespace Mirall
