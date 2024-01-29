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
