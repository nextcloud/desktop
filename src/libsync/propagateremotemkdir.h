/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include "owncloudpropagator.h"
#include "networkjobs.h"

namespace OCC {

class PropagateUploadEncrypted;

/**
 * @brief The PropagateRemoteMkdir class
 * @ingroup libsync
 */
class PropagateRemoteMkdir : public PropagateItemJob
{
    Q_OBJECT
    QPointer<AbstractNetworkJob> _job;
    bool _deleteExisting = false;
    PropagateUploadEncrypted *_uploadEncryptedHelper = nullptr;
    friend class PropagateDirectory; // So it can access the _item;
public:
    PropagateRemoteMkdir(OwncloudPropagator *propagator, const SyncFileItemPtr &item);

    void start() override;
    void abort(PropagatorJob::AbortType abortType) override;

    // Creating a directory should be fast.
    bool isLikelyFinishedQuickly() override { return true; }

    /**
     * Whether an existing entity with the same name may be deleted before
     * creating the directory.
     *
     * Default: false.
     */
    void setDeleteExisting(bool enabled);

private slots:
    void slotMkdir();
    void slotStartMkcolJob();
    void slotStartEncryptedMkcolJob(const QString &path, const QString &filename, quint64 size);
    void slotMkcolJobFinished();
    void slotEncryptFolderFinished(int status, EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus);
    void success();

private:
    void finalizeMkColJob(QNetworkReply::NetworkError err, const QString &jobHttpReasonPhraseString, const QString &jobPath);
};
}
