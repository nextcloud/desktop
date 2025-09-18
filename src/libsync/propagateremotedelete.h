/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudpropagator.h"
#include "networkjobs.h"

namespace OCC {

class DeleteJob;

class BasePropagateRemoteDeleteEncrypted;

/**
 * @brief The PropagateRemoteDelete class
 * @ingroup libsync
 */
class PropagateRemoteDelete : public PropagateItemJob
{
    Q_OBJECT
    QPointer<DeleteJob> _job;
    BasePropagateRemoteDeleteEncrypted *_deleteEncryptedHelper = nullptr;

public:
    PropagateRemoteDelete(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() override;
    void createDeleteJob(const QString &filename);
    void abort(PropagatorJob::AbortType abortType) override;

    bool isLikelyFinishedQuickly() override { return !_item->isDirectory(); }

private slots:
    void slotDeleteJobFinished();
};
}
