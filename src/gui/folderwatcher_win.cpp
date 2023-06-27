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

#include <QDir>
#include <QScopeGuard>
#include <QThread>

#include "common/asserts.h"
#include "common/utility.h"
#include "filesystem.h"
#include "folderwatcher.h"
#include "folderwatcher_win.h"

#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

namespace OCC {

WatcherThread::WatchChanges WatcherThread::watchChanges(size_t fileNotifyBufferSize)
{
    _directory = CreateFileW(
        reinterpret_cast<const wchar_t *>(_longPath.utf16()), // QString stores UTF-16 internally, so use that here.
        FILE_LIST_DIRECTORY,
        FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);

    if (_directory == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        qCWarning(lcFolderWatcher) << "Failed to create handle for" << _path << ", error:" << Utility::formatWinError(error);
        _directory = 0;
        return WatchChanges::Error;
    }

    QScopeGuard todoBeforeReturn([this]() {
        CancelIo(_directory);
        closeHandle();
    });

    OVERLAPPED overlapped;
    overlapped.hEvent = _resultEvent;

    // QVarLengthArray ensures the stack-buffer is aligned like double and qint64.
    QVarLengthArray<char, 4096 * 10> fileNotifyBuffer;
    fileNotifyBuffer.resize(static_cast<int>(fileNotifyBufferSize));

    while (true) {
        ResetEvent(_resultEvent);

        FILE_NOTIFY_INFORMATION *pFileNotifyBuffer =
                reinterpret_cast<FILE_NOTIFY_INFORMATION *>(fileNotifyBuffer.data());
        DWORD dwBytesReturned = 0;
        if (!ReadDirectoryChangesW(_directory, pFileNotifyBuffer,
                static_cast<DWORD>(fileNotifyBufferSize), true,
                FILE_NOTIFY_CHANGE_FILE_NAME
                    | FILE_NOTIFY_CHANGE_DIR_NAME
                    | FILE_NOTIFY_CHANGE_LAST_WRITE
                    | FILE_NOTIFY_CHANGE_ATTRIBUTES, // attributes are for vfs pin state changes
                &dwBytesReturned, &overlapped, nullptr)) {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
                qCDebug(lcFolderWatcher) << "The buffer for changes overflowed! Triggering a generic change and resizing";
                emit changed({_path});
                return WatchChanges::NeedBiggerBuffer;
            } else {
                qCWarning(lcFolderWatcher) << "ReadDirectoryChangesW error" << Utility::formatWinError(errorCode);
                return WatchChanges::Error;
            }
        }

        _parent->_ready = true;

        HANDLE handles[] = { _resultEvent, _stopEvent };
        DWORD result = WaitForMultipleObjects(
            2, handles,
            false, // awake once one of them arrives
            INFINITE);
        const auto error = GetLastError();
        if (result == 1) {
            qCDebug(lcFolderWatcher) << "Received stop event, aborting folder watcher thread";
            return WatchChanges::Done;
        }
        if (result != 0) {
            qCWarning(lcFolderWatcher) << "WaitForMultipleObjects failed" << result << Utility::formatWinError(error);
            return WatchChanges::Error;
        }

        bool ok = GetOverlappedResult(_directory, &overlapped, &dwBytesReturned, false);
        if (!ok) {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
                qCDebug(lcFolderWatcher) << "The buffer for changes overflowed! Triggering a generic change and resizing";
                emit lostChanges();
                emit changed({_path});
                return WatchChanges::NeedBiggerBuffer;
            } else {
                qCWarning(lcFolderWatcher) << "GetOverlappedResult error" << Utility::formatWinError(errorCode);
                return WatchChanges::Error;
            }
        }

        processEntries(pFileNotifyBuffer);
    }
}

void WatcherThread::processEntries(FILE_NOTIFY_INFORMATION *curEntry)
{
    QSet<QString> paths;
    while (curEntry) {
        const size_t fileNameBufferSize = 4096;
        TCHAR fileNameBuffer[fileNameBufferSize];

        const int len = curEntry->FileNameLength / sizeof(wchar_t);
        QString longfile = _longPath + QString::fromWCharArray(curEntry->FileName, len);

        // Unless the file was removed or renamed, get its full long name
        // TODO: We could still try expanding the path in the tricky cases...
        if (curEntry->Action != FILE_ACTION_REMOVED && curEntry->Action != FILE_ACTION_RENAMED_OLD_NAME) {
            const auto wfile = longfile.toStdWString();
            const int longNameSize = GetLongPathNameW(wfile.data(), fileNameBuffer, fileNameBufferSize);
            const auto error = GetLastError();
            if (longNameSize > 0) {
                longfile = QString::fromWCharArray(fileNameBuffer, longNameSize);
            } else {
                qCWarning(lcFolderWatcher) << "Error converting file name" << longfile << "to full length, keeping original name." << Utility::formatWinError(error);
            }
        }

        longfile = QDir::cleanPath(longfile);

        // Skip modifications of folders: One of these is triggered for changes
        // and new files in a folder, probably because of the folder's mtime
        // changing. We don't need them.
        const bool skip = curEntry->Action == FILE_ACTION_MODIFIED && QFileInfo(longfile).isDir();

        if (!skip) {
            paths.insert(longfile);
        }

        if (curEntry->NextEntryOffset == 0) {
            break;
        }
        // FILE_NOTIFY_INFORMATION has no fixed size and the offset is in bytes therefor we first need to cast to char
        curEntry = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(reinterpret_cast<char *>(curEntry) + curEntry->NextEntryOffset);
    }
    if (!paths.isEmpty()) {
        Q_EMIT changed(paths);
    }
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
    size_t bufferSize = 4096 * 10;
    const size_t maxBuffer = 64 * 1024;

    while (true) {
        switch (watchChanges(bufferSize)) {
        case WatchChanges::NeedBiggerBuffer:
            bufferSize = qMin(bufferSize * 2, maxBuffer);
            continue;
        case WatchChanges::Done:
            return;
        case WatchChanges::Error:
            // Other errors shouldn't actually happen,
            // so sleep a bit to avoid running into the same error case in a
            // tight loop.
            sleep(2);
        default:
            Q_UNREACHABLE();
        }
    }
}

WatcherThread::WatcherThread(FolderWatcherPrivate *parent, const QString &path)
    : QThread()
    , _parent(parent)
    , _path(path + (path.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")))
    , _longPath(FileSystem::longWinPath(_path))
    , _directory(0)
    , _resultEvent(0)
    , _stopEvent(0)
{
}

WatcherThread::~WatcherThread()
{
    closeHandle();
}

void WatcherThread::stop()
{
    SetEvent(_stopEvent);
}

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString &path)
    : _parent(p)
{
    _thread.reset(new WatcherThread(this, path));
    // we are using connects instead of directly emitting on p as we need to cross thread borders
    connect(_thread.get(), &WatcherThread::changed, _parent, &FolderWatcher::changeDetected, Qt::QueuedConnection);
    connect(_thread.get(), &WatcherThread::lostChanges, _parent, &FolderWatcher::lostChanges, Qt::QueuedConnection);
    _thread->start();
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
    _thread->stop();
    _thread->wait();
}

} // namespace OCC
