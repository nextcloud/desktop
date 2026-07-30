/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syncfileitem.h"
#include "common/checksums.h"
#include "common/syncjournalfilerecord.h"
#include "common/utility.h"
#include "helpers.h"
#include "filesystem.h"

#include <QLoggingCategory>
#include "csync/vio/csync_vio_local.h"

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcFileItem, "nextcloud.sync.fileitem", QtInfoMsg)

namespace EncryptionStatusEnums {

ItemEncryptionStatus fromDbEncryptionStatus(JournalDbEncryptionStatus encryptionStatus)
{
    auto result = ItemEncryptionStatus::NotEncrypted;

    switch (encryptionStatus)
    {
    case JournalDbEncryptionStatus::Encrypted:
        result = ItemEncryptionStatus::Encrypted;
        break;
    case JournalDbEncryptionStatus::EncryptedMigratedV1_2:
        result = ItemEncryptionStatus::EncryptedMigratedV1_2;
        break;
    case JournalDbEncryptionStatus::EncryptedMigratedV1_2Invalid:
        result = ItemEncryptionStatus::Encrypted;
        break;
    case JournalDbEncryptionStatus::EncryptedMigratedV2_0:
        result = ItemEncryptionStatus::EncryptedMigratedV2_0;
        break;
    case JournalDbEncryptionStatus::NotEncrypted:
        result = ItemEncryptionStatus::NotEncrypted;
        break;
    }

    return result;
}

JournalDbEncryptionStatus toDbEncryptionStatus(ItemEncryptionStatus encryptionStatus)
{
    auto result = JournalDbEncryptionStatus::NotEncrypted;

    switch (encryptionStatus)
    {
    case ItemEncryptionStatus::Encrypted:
        result = JournalDbEncryptionStatus::Encrypted;
        break;
    case ItemEncryptionStatus::EncryptedMigratedV1_2:
        result = JournalDbEncryptionStatus::EncryptedMigratedV1_2;
        break;
    case ItemEncryptionStatus::EncryptedMigratedV2_0:
        result = JournalDbEncryptionStatus::EncryptedMigratedV2_0;
        break;
    case ItemEncryptionStatus::NotEncrypted:
        result = JournalDbEncryptionStatus::NotEncrypted;
        break;
    }

    return result;
}

ItemEncryptionStatus fromEndToEndEncryptionApiVersion(const double version)
{
    if (version >= 2.0) {
        return ItemEncryptionStatus::EncryptedMigratedV2_0;
    } else if (version >= 1.2) {
        return ItemEncryptionStatus::EncryptedMigratedV1_2;
    } else if (version >= 1.0) {
        return ItemEncryptionStatus::Encrypted;
    } else {
        return ItemEncryptionStatus::NotEncrypted;
    }
}

}

SyncJournalFileRecord SyncFileItem::toSyncJournalFileRecordWithInode(const QString &localFileName) const
{
    SyncJournalFileRecord rec;
    rec._path = destination().toUtf8();
    rec._modtime = _modtime;

    // Some types should never be written to the database when propagation completes
    rec._type = _type;
    if (rec._type == ItemTypeVirtualFileDownload) {
        rec._type = ItemTypeFile;
        qCInfo(lcFileItem) << "Changing item type from ItemTypeVirtualFileDownload to normal file to avoid wrong record type in database" << rec._path;
    }
    if (rec._type == ItemTypeVirtualFileDehydration)
        rec._type = ItemTypeVirtualFile;

    rec._etag = _etag;
    rec._fileId = _fileId;
    rec._fileSize = _size;
    rec._remotePerm = _remotePerm;
    rec._isShared = _isShared;
    rec._sharedByMe = _sharedByMe;
    rec._lastShareStateFetchedTimestamp = _lastShareStateFetchedTimestamp;
    rec._serverHasIgnoredFiles = _serverHasIgnoredFiles;
    rec._checksumHeader = _checksumHeader;
    rec._e2eMangledName = _encryptedFileName.toUtf8();
    rec._e2eEncryptionStatus = EncryptionStatusEnums::toDbEncryptionStatus(_e2eEncryptionStatus);
    rec._lockstate._locked = _locked == LockStatus::LockedItem;
    rec._lockstate._lockOwnerDisplayName = _lockOwnerDisplayName;
    rec._lockstate._lockOwnerId = _lockOwnerId;
    rec._lockstate._lockOwnerType = static_cast<qint64>(_lockOwnerType);
    rec._lockstate._lockEditorApp = _lockEditorApp;
    rec._lockstate._lockTime = _lockTime;
    rec._lockstate._lockTimeout = _lockTimeout;
    rec._lockstate._lockToken = _lockToken;
    rec._isLivePhoto = _isLivePhoto;
    rec._livePhotoFile = _livePhotoFile;
    rec._folderQuota.bytesUsed = _folderQuota.bytesUsed;
    rec._folderQuota.bytesAvailable = _folderQuota.bytesAvailable;

    // Update the inode if possible
    rec._inode = _inode;
    if (FileSystem::getInode(localFileName, &rec._inode)) {
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
    item->_file = rec.path();
    item->_inode = rec._inode;
    item->_modtime = rec._modtime;
    item->_type = rec._type;
    item->_etag = rec._etag;
    item->_fileId = rec._fileId;
    item->_size = rec._fileSize;
    item->_remotePerm = rec._remotePerm;
    item->_serverHasIgnoredFiles = rec._serverHasIgnoredFiles;
    item->_checksumHeader = rec._checksumHeader;
    item->_encryptedFileName = rec.e2eMangledName();
    item->_e2eEncryptionStatus = EncryptionStatusEnums::fromDbEncryptionStatus(rec._e2eEncryptionStatus);
    item->_e2eEncryptionServerCapability = item->_e2eEncryptionStatus;
    item->_locked = rec._lockstate._locked ? LockStatus::LockedItem : LockStatus::UnlockedItem;
    item->_lockOwnerDisplayName = rec._lockstate._lockOwnerDisplayName;
    item->_lockOwnerId = rec._lockstate._lockOwnerId;
    item->_lockOwnerType = static_cast<LockOwnerType>(rec._lockstate._lockOwnerType);
    item->_lockEditorApp = rec._lockstate._lockEditorApp;
    item->_lockTime = rec._lockstate._lockTime;
    item->_lockTimeout = rec._lockstate._lockTimeout;
    item->_lockToken = rec._lockstate._lockToken;
    item->_sharedByMe = rec._sharedByMe;
    item->_isShared = rec._isShared;
    item->_lastShareStateFetchedTimestamp = rec._lastShareStateFetchedTimestamp;
    item->_isLivePhoto = rec._isLivePhoto;
    item->_livePhotoFile = rec._livePhotoFile;
    item->_folderQuota.bytesUsed = rec._folderQuota.bytesUsed;
    item->_folderQuota.bytesAvailable = rec._folderQuota.bytesAvailable;
    return item;
}

SyncFileItemPtr SyncFileItem::fromProperties(const QString &filePath, const QMap<QString, QString> &properties, RemotePermissions::MountedPermissionAlgorithm algorithm)
{
    SyncFileItemPtr item(new SyncFileItem);
    item->_file = filePath;
    item->_originalFile = filePath;

    const auto isDirectory = properties.value(QStringLiteral("resourcetype")).contains(QStringLiteral("collection"));
    item->_type = isDirectory ? ItemTypeDirectory : ItemTypeFile;

    item->_size = isDirectory ? 0 : properties.value(QStringLiteral("size")).toInt();
    item->_fileId = properties.value(QStringLiteral("id")).toUtf8();

    if (properties.contains(QStringLiteral("permissions"))) {
        item->_remotePerm = RemotePermissions::fromServerString(properties.value("permissions"), algorithm, properties);
    }

    if (!properties.value(QStringLiteral("share-types")).isEmpty()) {
        item->_remotePerm.setPermission(RemotePermissions::IsShared);
    }

    item->_isShared = item->_remotePerm.hasPermission(RemotePermissions::IsShared);
    item->_lastShareStateFetchedTimestamp = QDateTime::currentMSecsSinceEpoch();

    item->_e2eEncryptionStatus = (properties.value("is-encrypted"_L1) == "1"_L1 ? SyncFileItem::EncryptionStatus::EncryptedMigratedV2_0 : SyncFileItem::EncryptionStatus::NotEncrypted);
    if (item->isEncrypted()) {
        item->_e2eEncryptionServerCapability = item->_e2eEncryptionStatus;
    }
    item->_locked =
        properties.value("lock"_L1) == "1"_L1 ? SyncFileItem::LockStatus::LockedItem : SyncFileItem::LockStatus::UnlockedItem;
    item->_lockOwnerDisplayName = properties.value("lock-owner-displayname"_L1);
    item->_lockOwnerId = properties.value("lock-owner"_L1);
    item->_lockEditorApp = properties.value("lock-owner-editor"_L1);

    {
        auto ok = false;
        const auto intConvertedValue = properties.value("lock-owner-type"_L1).toULongLong(&ok);
        item->_lockOwnerType = ok ? static_cast<SyncFileItem::LockOwnerType>(intConvertedValue) : SyncFileItem::LockOwnerType::UserLock;
    }

    {
        auto ok = false;
        const auto intConvertedValue = properties.value("lock-time"_L1).toULongLong(&ok);
        item->_lockTime = ok ? intConvertedValue : 0;
    }

    {
        auto ok = false;
        const auto intConvertedValue = properties.value("lock-timeout"_L1).toULongLong(&ok);
        item->_lockTimeout = ok ? intConvertedValue : 0;
    }

    item->_lockToken = properties.value(QStringLiteral("lock-token"));

    auto getlastmodifiedValue = properties.value(QStringLiteral("getlastmodified"));
    getlastmodifiedValue.replace("GMT", "+0000");
    const auto date = QDateTime::fromString(getlastmodifiedValue, Qt::RFC2822Date);
    Q_ASSERT(date.isValid());
    if (date.toSecsSinceEpoch() > 0) {
        item->_modtime = date.toSecsSinceEpoch();
    }

    if (properties.contains(QStringLiteral("getetag"))) {
        item->_etag = parseEtag(properties.value(QStringLiteral("getetag")).toUtf8());
    }

    if (properties.contains(QStringLiteral("checksums"))) {
        item->_checksumHeader = findBestChecksum(properties.value("checksums").toUtf8());
    }

    if (properties.contains(QStringLiteral("metadata-files-live-photo"))) {
        item->_isLivePhoto = true;
        item->_livePhotoFile = properties.value(QStringLiteral("metadata-files-live-photo"));
    }

    if (isDirectory && properties.contains(FolderQuota::usedBytesC) && properties.contains(FolderQuota::availableBytesC)) {
        item->_folderQuota.bytesUsed = properties.value(FolderQuota::usedBytesC).toLongLong();
        item->_folderQuota.bytesAvailable = properties.value(FolderQuota::availableBytesC).toLongLong();
    }

    // direction and instruction are decided later
    item->_direction = SyncFileItem::None;
    item->_instruction = CSYNC_INSTRUCTION_NONE;

    return item;
}

void SyncFileItem::updateLockStateFromDbRecord(const SyncJournalFileRecord &dbRecord)
{
    _locked = dbRecord._lockstate._locked ? LockStatus::LockedItem : LockStatus::UnlockedItem;
    _lockOwnerId = dbRecord._lockstate._lockOwnerId;
    _lockOwnerDisplayName = dbRecord._lockstate._lockOwnerDisplayName;
    _lockOwnerType = static_cast<LockOwnerType>(dbRecord._lockstate._lockOwnerType);
    _lockEditorApp = dbRecord._lockstate._lockEditorApp;
    _lockTime = dbRecord._lockstate._lockTime;
    _lockTimeout = dbRecord._lockstate._lockTimeout;
    _lockToken = dbRecord._lockstate._lockToken;
}

}
