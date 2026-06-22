/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "basepropagateremotedeleteencrypted.h"

namespace OCC {

class PropagateRemoteDeleteEncrypted : public BasePropagateRemoteDeleteEncrypted
{
    Q_OBJECT
public:
    PropagateRemoteDeleteEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent);

    void start() override;

private:
    void slotFolderUnLockFinished(const QByteArray &folderId, int statusCode) override;
    void slotFetchMetadataJobFinished(int statusCode, const QString &message) override;
    void slotUpdateMetadataJobFinished(int statusCode, const QString &message) override;
};

}
