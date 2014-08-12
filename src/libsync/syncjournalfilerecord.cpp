/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "syncjournalfilerecord.h"
#include "syncfileitem.h"
#include "utility.h"

#include <qfileinfo.h>
#include <qdebug.h>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace Mirall {

SyncJournalFileRecord::SyncJournalFileRecord()
    :_inode(0), _type(0), _mode(0)
{
}

SyncJournalFileRecord::SyncJournalFileRecord(const SyncFileItem &item, const QString &localFileName)
    : _path(item._file), _modtime(Utility::qDateTimeFromTime_t(item._modtime)),
      _type(item._type), _etag(item._etag), _fileId(item._fileId), _remotePerm(item._remotePerm),
      _mode(0)
{
    // use the "old" inode coming with the item for the case where the
    // filesystem stat fails. That can happen if the the file was removed
    // or renamed meanwhile. For the rename case we still need the inode to
    // detect the rename tough.
    _inode = item._inode;

#ifdef Q_OS_WIN
    /* Query the inode:
       based on code from csync_vio_local.c (csync_vio_local_stat)
       Get the Windows file id as an inode replacement. */
    HANDLE h = CreateFileW( (wchar_t*)localFileName.utf16(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
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

SyncJournalBlacklistRecord::SyncJournalBlacklistRecord(const SyncFileItem& item, int retries)
    :_retryCount(retries), _errorString(item._errorString), _lastTryModtime(item._modtime)
    , _lastTryEtag(item._etag), _file(item._file)
{

}

}
