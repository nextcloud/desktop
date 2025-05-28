/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudpropagator.h"
#include "abstractnetworkjob.h"

#include <QLoggingCategory>
#include <QVector>

namespace OCC {

class PropagateDownloadEncrypted;

Q_DECLARE_LOGGING_CATEGORY(lcBulkPropagatorDownloadJob)

class BulkPropagatorDownloadJob : public PropagatorJob
{
    Q_OBJECT

public:
    explicit BulkPropagatorDownloadJob(OwncloudPropagator *propagator, PropagateDirectory *parentDirJob, const std::vector<SyncFileItemPtr> &items = {});

    bool scheduleSelfOrChild() override;

    [[nodiscard]] JobParallelism parallelism() const override;

public slots:
    void addDownloadItem(const OCC::SyncFileItemPtr &item);

    void start(const OCC::SyncFileItemPtr &item);

private slots:
    void startAfterIsEncryptedIsChecked(const OCC::SyncFileItemPtr &item);

    void finalizeOneFile(const OCC::SyncFileItemPtr &file);

    void done(const OCC::SyncFileItem::Status status);

    void abortWithError(OCC::SyncFileItemPtr item, OCC::SyncFileItem::Status status, const QString &error);

private:
    bool updateMetadata(const SyncFileItemPtr &item);

    void checkPropagationIsDone();

    std::vector<SyncFileItemPtr> _filesToDownload;

    std::vector<SyncFileItemPtr> _filesDownloading;

    PropagateDownloadEncrypted *_downloadEncryptedHelper = nullptr;

    PropagateDirectory *_parentDirJob = nullptr;
};

}
