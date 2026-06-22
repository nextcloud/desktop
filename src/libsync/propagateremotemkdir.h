/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    void slotEncryptFolderFinished(int status, OCC::EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus);
    void success();

private:
    void finalizeMkColJob(QNetworkReply::NetworkError err, const QString &jobHttpReasonPhraseString, const QString &jobPath);
};
}
