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

#pragma once

#include "encryptedfoldermetadatahandler.h" //NOTE: Forward declarion is not gonna work because of OWNCLOUDSYNC_EXPORT for UpdateE2eeFolderMetadataJob
#include "owncloudpropagator.h"
#include "syncfileitem.h"

#include <QScopedPointer>

class QNetworkReply;

namespace OCC {

class FolderMetadata;

class EncryptedFolderMetadataHandler;

class OWNCLOUDSYNC_EXPORT UpdateE2eeFolderMetadataJob : public PropagatorJob
{
    Q_OBJECT

public:
    explicit UpdateE2eeFolderMetadataJob(OwncloudPropagator *propagator, const SyncFileItemPtr &item, const QString &encryptedRemotePath);

    bool scheduleSelfOrChild() override;

    [[nodiscard]] JobParallelism parallelism() const override;

private slots:
    void start();
    void slotFetchMetadataJobFinished(int httpReturnCode, const QString &message);
    void slotUpdateMetadataFinished(int httpReturnCode, const QString &message);
    void unlockFolder(const EncryptedFolderMetadataHandler::UnlockFolderWithResult result);

signals:
    void fileDropMetadataParsedAndAdjusted(const OCC::FolderMetadata *const metadata);

private:
    SyncFileItemPtr _item;
    QString _encryptedRemotePath;

    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};

}
