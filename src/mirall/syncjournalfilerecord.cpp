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
#include <qfileinfo.h>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace Mirall {

SyncJournalFileRecord::SyncJournalFileRecord()
{
}

SyncJournalFileRecord::SyncJournalFileRecord(const SyncFileItem &item, const QString &localFileName)
    : _path(item._file), _type(item._type), _etag(item._etag)
{
    if (item._dir == SyncFileItem::Down) {
        QFileInfo fi(localFileName);
        // refersh modtime
        _modtime = fi.lastModified();
    } else {
        _modtime = QDateTime::fromTime_t(item._modtime);
    }

    // Query the inode:
    //   based on code from csync_vio_local.c (csync_vio_local_stat)
#ifdef _WIN32
    /* Get the Windows file id as an inode replacement. */
    HANDLE h = CreateFileW( (wchar_t*)localFileName.utf16(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, NULL );
    if( h == INVALID_HANDLE_VALUE ) {
        _inode = qrand();
        qDebug() << "Failed to query the 'inode' because CreateFileW failed for file " << localFileName;
    } else {
        FILETIME ftCreate, ftAccess, ftWrite;
        //     SYSTEMTIME stUTC;

        BY_HANDLE_FILE_INFORMATION fileInfo;

        if( GetFileInformationByHandle( h, &fileInfo ) ) {
            ULARGE_INTEGER FileIndex;
            FileIndex.HighPart = fileInfo.nFileIndexHigh;
            FileIndex.LowPart = fileInfo.nFileIndexLow;
            FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;

            /* printf("Index: %I64i\n", FileIndex.QuadPart); */

            _inode = FileIndex.QuadPart;
        } else {
            qDebug() << "Failed to query the 'inode' for file " << localFileName;
            _inode = qrand();
        }
    }
#else
    struct stat sb;
    if( stat(QFile::encodeName(localFileName).constData(), &sb) < 0) {
        qDebug() << "Failed to query the 'inode' for file " << localFileName;
        _inode = qrand();
    } else {
        _inode = sb.st_ino;
    }
#endif

}

}
