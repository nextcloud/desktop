/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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

class QNetworkReply;

namespace OCC {

class FolderMetadata;

class OWNCLOUDSYNC_EXPORT UpdateMigratedE2eeMetadataJob : public PropagateItemJob
{
    Q_OBJECT

public:
    explicit UpdateMigratedE2eeMetadataJob(OwncloudPropagator *propagator, const SyncFileItemPtr &syncFileItem, const QString &fullRemotePath, const QString &folderRemotePath);

    [[nodiscard]] bool scheduleSelfOrChild() override;

    [[nodiscard]] JobParallelism parallelism() const override;

    [[nodiscard]] QString fullRemotePath() const;

    void addSubJobItem(const QString &key, const SyncFileItemPtr &syncFileItem);

private slots:
    void start() override;

private:
    QHash<QString, SyncFileItemPtr> _subJobItems;
    QString _fullRemotePath;
    QString _folderRemotePath;
};

}
