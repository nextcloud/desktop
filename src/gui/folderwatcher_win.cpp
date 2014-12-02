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
#include <QDir>

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

    _handle = CreateFileW(
        (wchar_t*)_path.utf16(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
        );

    if (_handle == INVALID_HANDLE_VALUE)
    {
        DWORD errorCode = GetLastError();
        qDebug() << Q_FUNC_INFO << "Failed to create handle for" << _path << ", error:" << errorCode;
        _handle = 0;
        return;
    }

    // QVarLengthArray ensures the stack-buffer is aligned like double and qint64.
    QVarLengthArray<char, 4096*10> fileNotifyBuffer;
    fileNotifyBuffer.resize(fileNotifyBufferSize);

    const size_t fileNameBufferSize = 4096;
    TCHAR fileNameBuffer[fileNameBufferSize];

    forever {
        FILE_NOTIFY_INFORMATION *pFileNotifyBuffer =
                (FILE_NOTIFY_INFORMATION*)fileNotifyBuffer.data();
        DWORD dwBytesReturned = 0;
        SecureZeroMemory(pFileNotifyBuffer, fileNotifyBufferSize);
        if(ReadDirectoryChangesW( _handle, (LPVOID)pFileNotifyBuffer,
                                  fileNotifyBufferSize, true,
                                  FILE_NOTIFY_CHANGE_FILE_NAME |
                                  FILE_NOTIFY_CHANGE_DIR_NAME |
                                  FILE_NOTIFY_CHANGE_LAST_WRITE,
                                  &dwBytesReturned, NULL, NULL))
        {
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
                        qDebug() << Q_FUNC_INFO << "Error converting file name to full length, keeping original name.";
                    }
                }
                longfile = QDir::cleanPath(longfile);

                qDebug() << Q_FUNC_INFO << "Found change in" << longfile << "action:" << curEntry->Action;
                emit changed(longfile);

                if (curEntry->NextEntryOffset == 0) {
                    break;
                }
                curEntry = (FILE_NOTIFY_INFORMATION*)(
                                (char*)curEntry + curEntry->NextEntryOffset);
            }
        } else {
            DWORD errorCode = GetLastError();
            switch(errorCode) {
            case ERROR_NOTIFY_ENUM_DIR:
                qDebug() << Q_FUNC_INFO << "The buffer for changes overflowed! Triggering a generic change and resizing";
                emit changed(_path);
                *increaseBufferSize = true;
                break;
            default:
                qDebug() << Q_FUNC_INFO << "General error" << errorCode << "while watching. Exiting.";
                break;
            }
            CloseHandle(_handle);
            _handle = NULL;
            return;
        }
    }
}

void WatcherThread::run()
{
    // If this buffer fills up before we've extracted its data we will lose
    // change information. Therefore start big.
    size_t bufferSize = 4096*10;
    size_t maxBuffer = 64*1024;

    forever {
        bool increaseBufferSize = false;
        watchChanges(bufferSize, &increaseBufferSize);

        if (increaseBufferSize) {
            bufferSize = qMin(bufferSize*2, maxBuffer);
        } else {
            // Other errors shouldn't actually happen,
            // so sleep a bit to avoid running into the same error case in a
            // tight loop.
            sleep(2);
        }
    }
}

WatcherThread::~WatcherThread()
{
    if (_handle) {
        CloseHandle(_handle);
        _handle = NULL;
    }
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

} // namespace OCC
