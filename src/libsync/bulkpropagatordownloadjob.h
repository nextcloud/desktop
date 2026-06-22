/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudpropagator.h"
#include "abstractnetworkjob.h"

#include <QList>

namespace OCC {

class PropagateDownloadEncrypted;

class BulkPropagatorDownloadJob : public PropagatorJob
{
    Q_OBJECT

public:
    explicit BulkPropagatorDownloadJob(OwncloudPropagator *propagator, PropagateDirectory *parentDirJob);

    bool scheduleSelfOrChild() override;

    [[nodiscard]] JobParallelism parallelism() const override;

public slots:
    void addDownloadItem(const OCC::SyncFileItemPtr &item);

    void start();

private slots:
    void finalizeOneFile(const OCC::SyncFileItemPtr &file);

    void done(const OCC::SyncFileItem::Status status);

    void abortWithError(OCC::SyncFileItemPtr item, OCC::SyncFileItem::Status status, const QString &error);

private:
    bool updateMetadata(const SyncFileItemPtr &item);

    QList<SyncFileItemPtr> _filesToDownload;

    PropagateDownloadEncrypted *_downloadEncryptedHelper = nullptr;

    PropagateDirectory *_parentDirJob = nullptr;
};

}
