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

#include <qfileinfo.h>
#include <qdebug.h>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace OCC {

SyncJournalFileRecord::SyncJournalFileRecord()
    :_inode(0), _type(0), _fileSize(0), _serverHasIgnoredFiles(false)
{
}

SyncJournalFileRecord::SyncJournalFileRecord(const SyncFileItem &item, const QString &localFileName)
    : _path(item._file), _modtime(Utility::qDateTimeFromTime_t(item._modtime)),
      _type(item._type), _etag(item._etag), _fileId(item._fileId), _fileSize(item._size),
      _remotePerm(item._remotePerm), _serverHasIgnoredFiles(item._serverHasIgnoredFiles),
      _contentChecksum(item._contentChecksum),
      _contentChecksumType(item._contentChecksumType)
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

    HANDLE h = CreateFileW( (wchar_t*) FileSystem::longWinPath(localFileName).utf16(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, NULL );

    if( h == INVALID_HANDLE_VALUE ) {
        qWarning() << "Failed to query the 'inode' because CreateFileW failed for file " << localFileName;
    } else {
        BY_HANDLE_FILE_INFORMATION fileInfo;

        if( GetFileInformationByHandle( h, &fileInfo ) ) {
            ULARGE_INTEGER FileIndex;
            FileIndex.HighPart = fileInfo.nFileIndexHigh;
            FileIndex.LowPart = fileInfo.nFileIndexLow;
            FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;

            /* printf("Index: %I64i\n", FileIndex.QuadPart); */

            _inode = FileIndex.QuadPart;
        } else {
            qWarning() << "Failed to query the 'inode' for file " << localFileName;

        }
        CloseHandle(h);
    }
#else
    struct stat sb;
    if( stat(QFile::encodeName(localFileName).constData(), &sb) < 0) {
        qWarning() << "Failed to query the 'inode' for file " << localFileName;
    } else {
        _inode = sb.st_ino;
    }
#endif
    qDebug() << Q_FUNC_INFO << localFileName << "Retrieved inode " << _inode << "(previous item inode: " << item._inode << ")";

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
    item._contentChecksum = _contentChecksum;
    item._contentChecksumType = _contentChecksumType;
    return item;
}

static time_t getMinBlacklistTime()
{
    return qMax(qgetenv("OWNCLOUD_BLACKLIST_TIME_MIN").toInt(),
                25); // 25 seconds
}

static time_t getMaxBlacklistTime()
{
    int v = qgetenv("OWNCLOUD_BLACKLIST_TIME_MAX").toInt();
    if (v > 0)
        return v;
    return 24*60*60; // 1 day
}

bool SyncJournalErrorBlacklistRecord::isValid() const
{
    return ! _file.isEmpty()
        && (!_lastTryEtag.isEmpty() || _lastTryModtime != 0)
        && _lastTryTime > 0 && _ignoreDuration > 0;
}

SyncJournalErrorBlacklistRecord SyncJournalErrorBlacklistRecord::update(
        const SyncJournalErrorBlacklistRecord& old, const SyncFileItem& item)
{
    SyncJournalErrorBlacklistRecord entry;
    bool mayBlacklist =
            item._errorMayBeBlacklisted  // explicitly flagged for blacklisting
            || (item._httpErrorCode != 0 // or non-local error
#ifdef OWNCLOUD_5XX_NO_BLACKLIST
                && item._httpErrorCode / 100 != 5 // In this configuration, never blacklist error 5xx
#endif
               );

    if (!mayBlacklist) {
        qDebug() << "This error is not blacklisted " << item._httpErrorCode << item._errorMayBeBlacklisted;
        return entry;
    }

    static time_t minBlacklistTime(getMinBlacklistTime());
    static time_t maxBlacklistTime(qMax(getMaxBlacklistTime(), minBlacklistTime));

    entry._retryCount = old._retryCount + 1;
    entry._errorString = item._errorString;
    entry._lastTryModtime = item._modtime;
    entry._lastTryEtag = item._etag;
    entry._lastTryTime = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
    // The factor of 5 feels natural: 25s, 2 min, 10 min, ~1h, ~5h, ~24h
    entry._ignoreDuration = old._ignoreDuration * 5;
    entry._file = item._file;
    entry._renameTarget = item._renameTarget;

    if( item._httpErrorCode == 403 ) {
        qDebug() << "Probably firewall error: " << item._httpErrorCode << ", blacklisting up to 1h only";
        entry._ignoreDuration = qMin(entry._ignoreDuration, time_t(60*60));

    } else if( item._httpErrorCode == 413 || item._httpErrorCode == 415 ) {
        qDebug() << "Fatal Error condition" << item._httpErrorCode << ", maximum blacklist ignore time!";
        entry._ignoreDuration = maxBlacklistTime;
    }
    entry._ignoreDuration = qBound(minBlacklistTime, entry._ignoreDuration, maxBlacklistTime);

    qDebug() << "blacklisting " << item._file
             << " for " << entry._ignoreDuration
             << ", retry count " << entry._retryCount;
    return entry;
}


bool operator==(const SyncJournalFileRecord & lhs,
                const SyncJournalFileRecord & rhs)
{
    return     lhs._path == rhs._path
            && lhs._inode == rhs._inode
            && lhs._modtime.toTime_t() == rhs._modtime.toTime_t()
            && lhs._type == rhs._type
            && lhs._etag == rhs._etag
            && lhs._fileId == rhs._fileId
            && lhs._fileSize == rhs._fileSize
            && lhs._remotePerm == rhs._remotePerm
            && lhs._serverHasIgnoredFiles == rhs._serverHasIgnoredFiles
            && lhs._contentChecksum == rhs._contentChecksum
            && lhs._contentChecksumType == rhs._contentChecksumType;
}

}
