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
#include "filesystem.h"

#include "csync/vio/csync_vio_local.h"

#include <QCoreApplication>

namespace OCC {

Q_LOGGING_CATEGORY(lcFileItem, "sync.fileitem", QtInfoMsg)

SyncJournalFileRecord SyncFileItem::toSyncJournalFileRecordWithInode(const QString &localFileName) const
{
    SyncJournalFileRecord rec;
    rec._path = destination().toUtf8();
    rec._modtime = _modtime;

    // Some types should never be written to the database when propagation completes
    rec._type = _type;
    if (rec._type == ItemTypeVirtualFileDownload)
        rec._type = ItemTypeFile;
    if (rec._type == ItemTypeVirtualFileDehydration)
        rec._type = ItemTypeVirtualFile;

    rec._etag = _etag.toUtf8();
    rec._fileId = _fileId;
    rec._fileSize = _size;
    rec._remotePerm = _remotePerm;
    rec._serverHasIgnoredFiles = _serverHasIgnoredFiles;
    rec._checksumHeader = _checksumHeader;

    // Update the inode if possible
    rec._inode = _inode;
    if (FileSystem::getInode(localFileName, &rec._inode)) {
        qCDebug(lcFileItem) << localFileName << "Retrieved inode " << rec._inode << "(previous item inode: " << _inode << ")";
    } else {
        // use the "old" inode coming with the item for the case where the
        // filesystem stat fails. That can happen if the the file was removed
        // or renamed meanwhile. For the rename case we still need the inode to
        // detect the rename though.
        qCWarning(lcFileItem) << "Failed to query the 'inode' for file " << localFileName;
    }
    return rec;
}

SyncFileItemPtr SyncFileItem::fromSyncJournalFileRecord(const SyncJournalFileRecord &rec)
{
    auto item = SyncFileItemPtr::create();
    item->_file = QString::fromUtf8(rec._path);
    item->_inode = rec._inode;
    item->_modtime = rec._modtime;
    item->_type = rec._type;
    item->_etag = QString::fromUtf8(rec._etag);
    item->_fileId = rec._fileId;
    item->_size = rec._fileSize;
    item->_remotePerm = rec._remotePerm;
    item->_serverHasIgnoredFiles = rec._serverHasIgnoredFiles;
    item->_checksumHeader = rec._checksumHeader;
    return item;
}

template <>
QString Utility::enumToDisplayName(SyncFileItem::Status s)
{
    switch (s) {
    case SyncFileItem::NoStatus:
        return QCoreApplication::translate("SyncFileItem::Status", "Undefined");
    case OCC::SyncFileItem::FatalError:
        return QCoreApplication::translate("SyncFileItem::Status", "Fatal Error");
    case OCC::SyncFileItem::NormalError:
        return QCoreApplication::translate("SyncFileItem::Status", "Error");
    case OCC::SyncFileItem::SoftError:
        return QCoreApplication::translate("SyncFileItem::Status", "Info");
    case OCC::SyncFileItem::Success:
        return QCoreApplication::translate("SyncFileItem::Status", "Success");
    case OCC::SyncFileItem::Conflict:
        return QCoreApplication::translate("SyncFileItem::Status", "Conflict");
    case OCC::SyncFileItem::FileIgnored:
        return QCoreApplication::translate("SyncFileItem::Status", "File Ignored");
    case OCC::SyncFileItem::Restoration:
        return QCoreApplication::translate("SyncFileItem::Status", "Restored");
    case OCC::SyncFileItem::DetailError:
        return QCoreApplication::translate("SyncFileItem::Status", "Error");
    case OCC::SyncFileItem::BlacklistedError:
        return QCoreApplication::translate("SyncFileItem::Status", "Blacklisted");
    case OCC::SyncFileItem::Excluded:
        return QCoreApplication::translate("SyncFileItem::Status", "Excluded");
    case OCC::SyncFileItem::Message:
        return QCoreApplication::translate("SyncFileItem::Status", "Message");
    case OCC::SyncFileItem::FilenameReserved:
        return QCoreApplication::translate("SyncFileItem::Status", "Filename Reserved");
    case OCC::SyncFileItem::StatusCount:
        Q_UNREACHABLE();
    }
    Q_UNREACHABLE();
}
}

QDebug operator<<(QDebug debug, const OCC::SyncFileItem *item)
{
    if (!item) {
        debug << "OCC::SyncFileItem(0x0)";
    } else {
        QDebugStateSaver saver(debug);
        debug.setAutoInsertSpaces(false);
        debug << "OCC::SyncFileItem(file=" << item->_file;
        if (!item->_renameTarget.isEmpty()) {
            debug << ", destination=" << item->destination();
        }
        debug << ", type=" << item->_type << ", instruction=" << item->_instruction << ", status=" << item->_status << ")";
    }
    return debug;
}
