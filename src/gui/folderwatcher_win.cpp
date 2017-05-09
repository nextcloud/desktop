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
#include <QDir>

#include "filesystem.h"
#include "folderwatcher.h"
#include "folderwatcher_win.h"

#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

namespace OCC {

void WatcherThread::watchChanges(size_t fileNotifyBufferSize,
                                 bool* increaseBufferSize)
{
    *increaseBufferSize = false;
    QString longPath = FileSystem::longWinPath(_path);

    _directory = CreateFileW(
        (wchar_t*) longPath.utf16(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
        );

    if (_directory == INVALID_HANDLE_VALUE)
    {
        DWORD errorCode = GetLastError();
        qCDebug(lcFolderWatcher) << "Failed to create handle for" << _path << ", error:" << errorCode;
        _directory = 0;
        return;
    }

    OVERLAPPED overlapped;
    overlapped.hEvent = _resultEvent;

    // QVarLengthArray ensures the stack-buffer is aligned like double and qint64.
    QVarLengthArray<char, 4096*10> fileNotifyBuffer;
    fileNotifyBuffer.resize(fileNotifyBufferSize);

    const size_t fileNameBufferSize = 4096;
    TCHAR fileNameBuffer[fileNameBufferSize];


    while (!_done) {
        ResetEvent(_resultEvent);

        FILE_NOTIFY_INFORMATION *pFileNotifyBuffer =
                (FILE_NOTIFY_INFORMATION*)fileNotifyBuffer.data();
        DWORD dwBytesReturned = 0;
        SecureZeroMemory(pFileNotifyBuffer, fileNotifyBufferSize);
        if(! ReadDirectoryChangesW( _directory, (LPVOID)pFileNotifyBuffer,
                                    fileNotifyBufferSize, true,
                                    FILE_NOTIFY_CHANGE_FILE_NAME |
                                    FILE_NOTIFY_CHANGE_DIR_NAME |
                                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                                    &dwBytesReturned,
                                    &overlapped,
                                    NULL))
        {
            DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
                qCDebug(lcFolderWatcher) << "The buffer for changes overflowed! Triggering a generic change and resizing";
                emit changed(_path);
                *increaseBufferSize = true;
            } else {
                qCDebug(lcFolderWatcher) << "ReadDirectoryChangesW error" << errorCode;
            }
            break;
        }

        HANDLE handles[] = {_resultEvent, _stopEvent};
        DWORD result = WaitForMultipleObjects(
                    2, handles,
                    false, // awake once one of them arrives
                    INFINITE);
        if (result == 1) {
            qCDebug(lcFolderWatcher) << "Received stop event, aborting folder watcher thread";
            break;
        }
        if (result != 0) {
            qCDebug(lcFolderWatcher) << "WaitForMultipleObjects failed" << result << GetLastError();
            break;
        }

        bool ok = GetOverlappedResult(_directory, &overlapped, &dwBytesReturned, false);
        if (! ok) {
            DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
                qCDebug(lcFolderWatcher) << "The buffer for changes overflowed! Triggering a generic change and resizing";
                emit changed(_path);
                *increaseBufferSize = true;
            } else {
                qCDebug(lcFolderWatcher) << "GetOverlappedResult error" << errorCode;
            }
            break;
        }

        FILE_NOTIFY_INFORMATION *curEntry = pFileNotifyBuffer;
        forever {
            size_t len = curEntry->FileNameLength / 2;
            QString file = _path + "\\" + QString::fromWCharArray(curEntry->FileName, len);

            // Unless the file was removed or renamed, get its full long name
            // TODO: We could still try expanding the path in the tricky cases...
            QString longfile = file;
            if (curEntry->Action != FILE_ACTION_REMOVED
                    && curEntry->Action != FILE_ACTION_RENAMED_OLD_NAME) {
                size_t longNameSize = GetLongPathNameW(reinterpret_cast<LPCWSTR>(file.utf16()), fileNameBuffer, fileNameBufferSize);
                if (longNameSize > 0) {
                    longfile = QString::fromUtf16(reinterpret_cast<const ushort *>(fileNameBuffer), longNameSize);
                } else {
                    qCDebug(lcFolderWatcher) << "Error converting file name to full length, keeping original name.";
                }
            }
            longfile = QDir::cleanPath(longfile);

            // Skip modifications of folders: One of these is triggered for changes
            // and new files in a folder, probably because of the folder's mtime
            // changing. We don't need them.
            bool skip = curEntry->Action == FILE_ACTION_MODIFIED
                    && QFileInfo(longfile).isDir();

            if (!skip) {
                emit changed(longfile);
            }

            if (curEntry->NextEntryOffset == 0) {
                break;
            }
            curEntry = (FILE_NOTIFY_INFORMATION*)(
                            (char*)curEntry + curEntry->NextEntryOffset);
        }
    }

    CancelIo(_directory);
    closeHandle();
}

void WatcherThread::closeHandle()
{
    if (_directory) {
        CloseHandle(_directory);
        _directory = NULL;
    }
}

void WatcherThread::run()
{
    _resultEvent = CreateEvent(NULL, true, false, NULL);
    _stopEvent = CreateEvent(NULL, true, false, NULL);

    // If this buffer fills up before we've extracted its data we will lose
    // change information. Therefore start big.
    size_t bufferSize = 4096*10;
    size_t maxBuffer = 64*1024;

    while (!_done) {
        bool increaseBufferSize = false;
        watchChanges(bufferSize, &increaseBufferSize);

        if (increaseBufferSize) {
            bufferSize = qMin(bufferSize*2, maxBuffer);
        } else if (!_done) {
            // Other errors shouldn't actually happen,
            // so sleep a bit to avoid running into the same error case in a
            // tight loop.
            sleep(2);
        }
    }
}

WatcherThread::~WatcherThread()
{
    closeHandle();
}

void WatcherThread::stop()
{
    _done = 1;
    SetEvent(_stopEvent);
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
    _thread->stop();
    _thread->wait();
    delete _thread;
}

} // namespace OCC
