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
    _handle = CreateFileW(
        (wchar_t*)_path.utf16(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
        );

    size_t bufsize = 4096;
    size_t maxlen = 4096;
    while(true) {
        char fileNotify[bufsize];
        // TODO: handle CreateFileW failure
        FILE_NOTIFY_INFORMATION *pFileNotify =
                (FILE_NOTIFY_INFORMATION*)fileNotify;
        DWORD dwBytesReturned = 0;
        SecureZeroMemory(pFileNotify, bufsize);
        if(ReadDirectoryChangesW( _handle, (LPVOID)pFileNotify,
                                  bufsize, true,
                                  FILE_NOTIFY_CHANGE_LAST_WRITE |
                                  FILE_NOTIFY_CHANGE_DIR_NAME |
                                  FILE_NOTIFY_CHANGE_LAST_WRITE,
                                  &dwBytesReturned, NULL, NULL))
        {
            FILE_NOTIFY_INFORMATION *curEntry = pFileNotify;
            while(true) {
                size_t len = pFileNotify->FileNameLength / 2;
                QString file = _path + "\\" + QString::fromWCharArray(pFileNotify->FileName, len);

                QString longfile;
                QScopedArrayPointer<TCHAR> buffer(new TCHAR[maxlen]);
                if (GetLongPathNameW(reinterpret_cast<LPCWSTR>(file.utf16()), buffer.data(), maxlen) == 0) {
                    qDebug() << Q_FUNC_INFO << "Error converting file name to full length, resorting to original name.";
                    longfile = file;
                } else {
                    longfile = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), maxlen-1);
                }

                qDebug() << Q_FUNC_INFO << "Found change in" << file;
                emit changed(longfile);
                if (curEntry->NextEntryOffset == 0) {
                    break;
                }
                curEntry = (FILE_NOTIFY_INFORMATION*)
                                (char*)curEntry + curEntry->NextEntryOffset;
            }
        } else {
            switch(GetLastError()) {
            case ERROR_NOTIFY_ENUM_DIR:
                qDebug() << Q_FUNC_INFO << "Too many events for buffer, resizing";
                bufsize *= 2;
                break;
            default:
                qDebug() << Q_FUNC_INFO << "General error while watching. Exiting.";
                CloseHandle(_handle);
                _handle = NULL;
                break;
            }
        }
    }
}

WatcherThread::~WatcherThread()
{
    if (_handle)
        FindCloseChangeNotification(_handle);
}

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString& path)
    : _parent(p)
{
    _thread = new WatcherThread(path);
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
