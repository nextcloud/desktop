/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "fileactivitylistmodel.h"
#include "folderman.h"
#include "tray/activitylistmodel.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFileActivityListModel, "nextcloud.gui.fileactivitylistmodel", QtInfoMsg)

FileActivityListModel::FileActivityListModel(QObject *parent)
    : ActivityListModel(nullptr, parent)
{
    setDisplayActions(false);
}

void FileActivityListModel::load(AccountState *accountState, const QString &localPath)
{
    Q_ASSERT(accountState);
    if (!accountState || currentlyFetching()) {
        return;
    }
    setAccountState(accountState);

    const auto folder = FolderMan::instance()->folderForPath(localPath);
    if (!folder) {
        return;
    }

    const auto file = folder->fileFromLocalPath(localPath);
    SyncJournalFileRecord fileRecord;
    if (!folder->journalDb()->getFileRecord(file, &fileRecord) || !fileRecord.isValid()) {
        return;
    }

    _fileId = fileRecord._fileId;
    slotRefreshActivity();
}

void FileActivityListModel::startFetchJob()
{
    if (!accountState()->isConnected()) {
        return;
    }
    setCurrentlyFetching(true);

    const QString url(QStringLiteral("ocs/v2.php/apps/activity/api/v2/activity/filter"));
    auto job = new JsonApiJob(accountState()->account(), url, this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &FileActivityListModel::activitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("sort"), QStringLiteral("asc"));
    params.addQueryItem(QStringLiteral("object_type"), "files");
    params.addQueryItem(QStringLiteral("object_id"), _fileId);
    job->addQueryParams(params);
    setDoneFetching(true);
    setHideOldActivities(true);
    job->start();
}
}
