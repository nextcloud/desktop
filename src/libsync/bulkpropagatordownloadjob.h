/*
 * Copyright (C) 2024 by Oleksandr Zolotov <alex@nextcloud.com>
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
    void addDownloadItem(const SyncFileItemPtr &item);
    void start(const SyncFileItemPtr &item);

private slots:
    void startAfterIsEncryptedIsChecked(const SyncFileItemPtr &item);

    void finalizeOneFile(const SyncFileItemPtr &file);

    void done( const SyncFileItem::Status status);

    void abortWithError(SyncFileItemPtr item, SyncFileItem::Status status, const QString &error);

private:
    bool updateMetadata(const SyncFileItemPtr &item);
    void checkPropagationIsDone();

    std::vector<SyncFileItemPtr> _filesToDownload;
    std::vector<SyncFileItemPtr> _filesDownloading;

    PropagateDownloadEncrypted *_downloadEncryptedHelper = nullptr;

    PropagateDirectory *_parentDirJob = nullptr;
};

}
