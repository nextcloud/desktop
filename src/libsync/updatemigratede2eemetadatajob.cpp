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

#include "updatemigratede2eemetadatajob.h"
#include "updatee2eefolderusersmetadatajob.h"

#include "account.h"
#include "syncfileitem.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcUpdateMigratedE2eeMetadataJob, "nextcloud.sync.propagator.updatemigratede2eemetadatajob", QtInfoMsg)

}

namespace OCC {

UpdateMigratedE2eeMetadataJob::UpdateMigratedE2eeMetadataJob(OwncloudPropagator *propagator,
                                                             const SyncFileItemPtr &syncFileItem,
                                                             const QString &path,
                                                             const QString &folderRemotePath)
    : PropagatorJob(propagator)
    , _item(syncFileItem)
    , _path(path)
    , _folderRemotePath(folderRemotePath)
{
}

void UpdateMigratedE2eeMetadataJob::start()
{
    const auto updateMedatadaAndSubfoldersJob = new UpdateE2eeFolderUsersMetadataJob(propagator()->account(),
                                                                                     propagator()->_journal,
                                                                                     _folderRemotePath,
                                                                                     UpdateE2eeFolderUsersMetadataJob::Add,
                                                                                     _path,
                                                                                     propagator()->account()->davUser(),
                                                                                     propagator()->account()->e2e()->_certificate);
    updateMedatadaAndSubfoldersJob->setParent(this);
    updateMedatadaAndSubfoldersJob->setSubJobSyncItems(_subJobItems);
    _subJobItems.clear();
    updateMedatadaAndSubfoldersJob->start();
    connect(updateMedatadaAndSubfoldersJob, &UpdateE2eeFolderUsersMetadataJob::finished, this, [this, updateMedatadaAndSubfoldersJob](const int code, const QString& message) {
        if (code == 200) {
            _item->_e2eEncryptionStatus = updateMedatadaAndSubfoldersJob->encryptionStatus();
            _item->_e2eEncryptionStatusRemote = updateMedatadaAndSubfoldersJob->encryptionStatus();
            emit finished(SyncFileItem::Status::Success);
        } else {
            _item->_errorString = message;
            emit finished(SyncFileItem::Status::NormalError);
        }
    });
}

bool UpdateMigratedE2eeMetadataJob::scheduleSelfOrChild()
{
    if (_state == Finished) {
        return false;
    }

    if (_state == NotYetStarted) {
        _state = Running;
        start();
    }

    return true;
}

PropagatorJob::JobParallelism UpdateMigratedE2eeMetadataJob::parallelism() const
{
    return PropagatorJob::JobParallelism::WaitForFinished;
}

QString UpdateMigratedE2eeMetadataJob::path() const
{
    return _path;
}

void UpdateMigratedE2eeMetadataJob::addSubJobItem(const QString &key, const SyncFileItemPtr &syncFileItem)
{
    _subJobItems.insert(key, syncFileItem);
}

}
