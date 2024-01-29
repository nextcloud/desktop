/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "propagateremotedeleteencrypted.h"
#include "clientsideencryptionjobs.h"
#include "foldermetadata.h"
#include "owncloudpropagator.h"
#include "encryptfolderjob.h"
#include <QLoggingCategory>
#include <QFileInfo>

using namespace OCC;

Q_LOGGING_CATEGORY(PROPAGATE_REMOVE_ENCRYPTED, "nextcloud.sync.propagator.remove.encrypted")

PropagateRemoteDeleteEncrypted::PropagateRemoteDeleteEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent)
    : BasePropagateRemoteDeleteEncrypted(propagator, item, parent)
{

}

void PropagateRemoteDeleteEncrypted::start()
{
    Q_ASSERT(!_item->_encryptedFileName.isEmpty());

    const QFileInfo info(_item->_encryptedFileName);
    fetchMetadataForPath(info.path());
}

void PropagateRemoteDeleteEncrypted::slotFolderUnLockFinished(const QByteArray &folderId, int statusCode)
{
    BasePropagateRemoteDeleteEncrypted::slotFolderUnLockFinished(folderId, statusCode);
    emit finished(!_isTaskFailed);
}

void PropagateRemoteDeleteEncrypted::slotFetchMetadataJobFinished(int statusCode, const QString &message)
{
    Q_UNUSED(message);
    if (statusCode == 404) {
        qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Metadata not found, but let's proceed with removing the file anyway.";
        deleteRemoteItem(_item->_encryptedFileName);
        return;
    }

    const auto metadata = folderMetadata();
    if (!metadata || !metadata->isValid()) {
        taskFailed();
        return;
    }

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Metadata Received, preparing it for removal of the file";

    const QFileInfo info(_propagator->fullLocalPath(_item->_file));
    const QString fileName = info.fileName();

    // Find existing metadata for this file
    bool found = false;
    const QVector<FolderMetadata::EncryptedFile> files = metadata->files();
    for (const FolderMetadata::EncryptedFile &file : files) {
        if (file.originalFilename == fileName) {
            metadata->removeEncryptedFile(file);
            found = true;
            break;
        }
    }

    if (!found) {
        // file is not found in the metadata, but we still need to remove it
        deleteRemoteItem(_item->_encryptedFileName);
        return;
    }

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Metadata updated, sending to the server.";
    uploadMetadata(EncryptedFolderMetadataHandler::UploadMode::KeepLock);
}

void PropagateRemoteDeleteEncrypted::slotUpdateMetadataJobFinished(int statusCode, const QString &message)
{
    Q_UNUSED(statusCode);
    Q_UNUSED(message);
    deleteRemoteItem(_item->_encryptedFileName);
}
