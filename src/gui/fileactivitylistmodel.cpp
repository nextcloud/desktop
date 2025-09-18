/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    connect(this, &FileActivityListModel::accountStateChanged, this, &FileActivityListModel::load);
}

QString FileActivityListModel::localPath() const
{
    return _localPath;
}

void FileActivityListModel::setLocalPath(const QString &localPath)
{
    if(localPath == _localPath) {
        return;
    }

    _localPath = localPath;
    Q_EMIT localPathChanged();

    load();
}

void FileActivityListModel::load()
{
    if (!accountState() || _localPath.isEmpty() || currentlyFetching()) {
        return;
    }

    const auto folder = FolderMan::instance()->folderForPath(_localPath);

    if (!folder) {
        qCWarning(lcFileActivityListModel) << "Invalid folder for localPath:" << _localPath << "will not load activity list model.";
        return;
    }

    const auto folderRelativePath = _localPath.mid(folder->cleanPath().length() + 1);
    SyncJournalFileRecord record;

    if (!folder->journalDb()->getFileRecord(folderRelativePath, &record) || !record.isValid()) {
        qCWarning(lcFileActivityListModel) << "Invalid file record for path:" << _localPath << "will not load activity list model.";
        return;
    }

    _objectId = record.numericFileId().toInt();
    slotRefreshActivity();
}

void FileActivityListModel::startFetchJob()
{
    if (!accountState()->isConnected() || _objectId == -1) {
        return;
    }
    setAndRefreshCurrentlyFetching(true);

    const QString url(QStringLiteral("ocs/v2.php/apps/activity/api/v2/activity/filter"));
    auto job = new JsonApiJob(accountState()->account(), url, this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &FileActivityListModel::activitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("sort"), QStringLiteral("asc"));
    params.addQueryItem(QStringLiteral("object_type"), "files");
    params.addQueryItem(QStringLiteral("object_id"), QString::number(_objectId));
    job->addQueryParams(params);
    setDoneFetching(true);
    setHideOldActivities(true);
    job->start();
}
}
