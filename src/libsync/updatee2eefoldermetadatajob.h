/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    void unlockFolder(const OCC::EncryptedFolderMetadataHandler::UnlockFolderWithResult result);

signals:
    void fileDropMetadataParsedAndAdjusted(const OCC::FolderMetadata *const metadata);

private:
    SyncFileItemPtr _item;
    QString _encryptedRemotePath;

    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};

}
