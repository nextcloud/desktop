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

#include "syncfileitem.h"
#include "common/syncjournalfilerecord.h"
#include "common/utility.h"

#include <QLoggingCategory>
#include "csync/vio/csync_vio_local.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFileItem, "sync.fileitem", QtInfoMsg)

SyncJournalFileRecord SyncFileItem::toSyncJournalFileRecordWithInode(const QString &localFileName)
{
    SyncJournalFileRecord rec;
    rec._path = _file.toUtf8();
    rec._modtime = _modtime;
    rec._type = _type;
    rec._etag = _etag;
    rec._fileId = _fileId;
    rec._fileSize = _size;
    rec._remotePerm = _remotePerm;
    rec._serverHasIgnoredFiles = _serverHasIgnoredFiles;
    rec._checksumHeader = _checksumHeader;

    // Go through csync vio just to get the inode.
    csync_file_stat_t fs;
    if (csync_vio_local_stat(localFileName.toUtf8().constData(), &fs) == 0) {
        rec._inode = fs.inode;
        qCDebug(lcFileItem) << localFileName << "Retrieved inode " << _inode << "(previous item inode: " << _inode << ")";
    } else {
        // use the "old" inode coming with the item for the case where the
        // filesystem stat fails. That can happen if the the file was removed
        // or renamed meanwhile. For the rename case we still need the inode to
        // detect the rename though.
        rec._inode = _inode;
        qCWarning(lcFileItem) << "Failed to query the 'inode' for file " << localFileName;
    }
    return rec;
}

SyncFileItemPtr SyncFileItem::fromSyncJournalFileRecord(const SyncJournalFileRecord &rec)
{
    SyncFileItemPtr item(new SyncFileItem);
    item->_file = rec._path;
    item->_inode = rec._inode;
    item->_modtime = rec._modtime;
    item->_type = static_cast<SyncFileItem::Type>(rec._type);
    item->_etag = rec._etag;
    item->_fileId = rec._fileId;
    item->_size = rec._fileSize;
    item->_remotePerm = rec._remotePerm;
    item->_serverHasIgnoredFiles = rec._serverHasIgnoredFiles;
    item->_checksumHeader = rec._checksumHeader;
    return item;
}

}
