/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "syncjournalfilerecord.h"
#include "syncfileitem.h"
#include "utility.h"
#include "filesystem.h"

#include <QLoggingCategory>
#include <qfileinfo.h>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcFileRecord, "sync.database.filerecord", QtInfoMsg)

SyncJournalFileRecord::SyncJournalFileRecord()
    : _inode(0)
    , _type(0)
    , _fileSize(0)
    , _serverHasIgnoredFiles(false)
{
}

SyncJournalFileRecord::SyncJournalFileRecord(const SyncFileItem &item, const QString &localFileName)
    : _path(item._file)
    , _modtime(Utility::qDateTimeFromTime_t(item._modtime))
    , _type(item._type)
    , _etag(item._etag)
    , _fileId(item._fileId)
    , _fileSize(item._size)
    , _remotePerm(item._remotePerm)
    , _serverHasIgnoredFiles(item._serverHasIgnoredFiles)
    , _checksumHeader(item._checksumHeader)
{
    // use the "old" inode coming with the item for the case where the
    // filesystem stat fails. That can happen if the the file was removed
    // or renamed meanwhile. For the rename case we still need the inode to
    // detect the rename though.
    _inode = item._inode;

#ifdef Q_OS_WIN
    /* Query the inode:
       based on code from csync_vio_local.c (csync_vio_local_stat)
       Get the Windows file id as an inode replacement. */

    HANDLE h = CreateFileW((wchar_t *)FileSystem::longWinPath(localFileName).utf16(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL + FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        qCWarning(lcFileRecord) << "Failed to query the 'inode' because CreateFileW failed for file " << localFileName;
    } else {
        BY_HANDLE_FILE_INFORMATION fileInfo;

        if (GetFileInformationByHandle(h, &fileInfo)) {
            ULARGE_INTEGER FileIndex;
            FileIndex.HighPart = fileInfo.nFileIndexHigh;
            FileIndex.LowPart = fileInfo.nFileIndexLow;
            FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;

            /* printf("Index: %I64i\n", FileIndex.QuadPart); */

            _inode = FileIndex.QuadPart;
        } else {
            qCWarning(lcFileRecord) << "Failed to query the 'inode' for file " << localFileName;
        }
        CloseHandle(h);
    }
#else
    struct stat sb;
    if (stat(QFile::encodeName(localFileName).constData(), &sb) < 0) {
        qCWarning(lcFileRecord) << "Failed to query the 'inode' for file " << localFileName;
    } else {
        _inode = sb.st_ino;
    }
#endif
    qCDebug(lcFileRecord) << localFileName << "Retrieved inode " << _inode << "(previous item inode: " << item._inode << ")";
}

SyncFileItem SyncJournalFileRecord::toSyncFileItem()
{
    SyncFileItem item;
    item._file = _path;
    item._inode = _inode;
    item._modtime = Utility::qDateTimeToTime_t(_modtime);
    item._type = static_cast<SyncFileItem::Type>(_type);
    item._etag = _etag;
    item._fileId = _fileId;
    item._size = _fileSize;
    item._remotePerm = _remotePerm;
    item._serverHasIgnoredFiles = _serverHasIgnoredFiles;
    item._checksumHeader = _checksumHeader;
    return item;
}

QByteArray SyncJournalFileRecord::numericFileId() const
{
    // Use the id up until the first non-numeric character
    for (int i = 0; i < _fileId.size(); ++i) {
        if (_fileId[i] < '0' || _fileId[i] > '9') {
            return _fileId.left(i);
        }
    }
    return _fileId;
}

SyncJournalErrorBlacklistRecord SyncJournalErrorBlacklistRecord::fromSyncFileItem(
    const SyncFileItem &item)
{
    SyncJournalErrorBlacklistRecord record;
    record._file = item._file;
    record._errorString = item._errorString;
    record._lastTryModtime = item._modtime;
    record._lastTryEtag = item._etag;
    record._lastTryTime = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
    record._renameTarget = item._renameTarget;
    record._retryCount = 1;
    return record;
}

bool SyncJournalErrorBlacklistRecord::isValid() const
{
    return !_file.isEmpty()
        && (!_lastTryEtag.isEmpty() || _lastTryModtime != 0)
        && _lastTryTime > 0;
}

bool operator==(const SyncJournalFileRecord &lhs,
    const SyncJournalFileRecord &rhs)
{
    return lhs._path == rhs._path
        && lhs._inode == rhs._inode
        && lhs._modtime.toTime_t() == rhs._modtime.toTime_t()
        && lhs._type == rhs._type
        && lhs._etag == rhs._etag
        && lhs._fileId == rhs._fileId
        && lhs._fileSize == rhs._fileSize
        && lhs._remotePerm == rhs._remotePerm
        && lhs._serverHasIgnoredFiles == rhs._serverHasIgnoredFiles
        && lhs._checksumHeader == rhs._checksumHeader;
}
}
