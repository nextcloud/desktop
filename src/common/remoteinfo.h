/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef REMOTEINFO_H
#define REMOTEINFO_H

#include "folderquota.h"
#include "common/remotepermissions.h"
#include "common/syncitemenums.h"

#include <QString>
#include <QByteArray>

namespace OCC {

/**
 * Represent all the meta-data about a file in the server
 */
struct RemoteInfo
{
    /** FileName of the entry (this does not contains any directory or path, just the plain name */
    QString name;
    QByteArray etag;
    QByteArray fileId;
    QByteArray checksumHeader;
    OCC::RemotePermissions remotePerm;
    time_t modtime = 0;
    int64_t size = 0;
    int64_t sizeOfFolder = 0;
    bool isDirectory = false;
    bool _isE2eEncrypted = false;
    bool isFileDropDetected = false;
    QString e2eMangledName;
    bool sharedByMe = false;

    [[nodiscard]] bool isValid() const { return !name.isNull(); }
    [[nodiscard]] bool isE2eEncrypted() const { return _isE2eEncrypted; }

    QString directDownloadUrl;
    QString directDownloadCookies;

    SyncFileItemEnums::LockStatus locked = SyncFileItemEnums::LockStatus::UnlockedItem;
    QString lockOwnerDisplayName;
    QString lockOwnerId;
    SyncFileItemEnums::LockOwnerType lockOwnerType = SyncFileItemEnums::LockOwnerType::UserLock;
    QString lockEditorApp;
    qint64 lockTime = 0;
    qint64 lockTimeout = 0;
    QString lockToken;

    bool isLivePhoto = false;
    QString livePhotoFile;

    bool requestUpload = false;

    FolderQuota folderQuota;
};

}

#endif // REMOTEINFO_H
