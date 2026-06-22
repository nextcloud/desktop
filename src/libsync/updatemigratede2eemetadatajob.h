/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
