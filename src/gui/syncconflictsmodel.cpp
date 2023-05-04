/*
 * Copyright (C) 2023 by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include "syncconflictsmodel.h"
#include "folderman.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcSyncConflictsModel, "nextcloud.syncconflictsmodel", QtInfoMsg)

SyncConflictsModel::SyncConflictsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SyncConflictsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return mData.size();
}

QVariant SyncConflictsModel::data(const QModelIndex &index, int role) const
{
    auto result = QVariant{};

    Q_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid));

    if (index.parent().isValid()) {
        return result;
    }

    if (role >= static_cast<int>(SyncConflictRoles::ExistingFileName) && role <= static_cast<int>(SyncConflictRoles::ConflictPreviewUrl)) {
        auto convertedRole = static_cast<SyncConflictRoles>(role);

        switch (convertedRole) {
        case SyncConflictRoles::ExistingFileName:
            result = mConflictData[index.row()].mExistingFileName;
            break;
        case SyncConflictRoles::ExistingSize:
            result = mConflictData[index.row()].mExistingSize;
            break;
        case SyncConflictRoles::ConflictSize:
            result = mConflictData[index.row()].mConflictSize;
            break;
        case SyncConflictRoles::ExistingDate:
            result = mConflictData[index.row()].mExistingDate;
            break;
        case SyncConflictRoles::ConflictDate:
            result = mConflictData[index.row()].mConflictDate;
            break;
        case SyncConflictRoles::ExistingSelected:
            result = mConflictData[index.row()].mExistingSelected;
            break;
        case SyncConflictRoles::ConflictSelected:
            result = mConflictData[index.row()].mConflictSelected;
            break;
        case SyncConflictRoles::ExistingPreviewUrl:
            result = mConflictData[index.row()].mExistingPreviewUrl;
            break;
        case SyncConflictRoles::ConflictPreviewUrl:
            result = mConflictData[index.row()].mConflictPreviewUrl;
            break;
        }
    }

    return result;
}

QHash<int, QByteArray> SyncConflictsModel::roleNames() const
{
    auto result = QAbstractListModel::roleNames();

    result[static_cast<int>(SyncConflictRoles::ExistingFileName)] = "existingFileName";
    result[static_cast<int>(SyncConflictRoles::ExistingSize)] = "existingSize";
    result[static_cast<int>(SyncConflictRoles::ConflictSize)] = "conflictSize";
    result[static_cast<int>(SyncConflictRoles::ExistingDate)] = "existingDate";
    result[static_cast<int>(SyncConflictRoles::ConflictDate)] = "conflictDate";
    result[static_cast<int>(SyncConflictRoles::ExistingSelected)] = "existingSelected";
    result[static_cast<int>(SyncConflictRoles::ConflictSelected)] = "conflictSelected";
    result[static_cast<int>(SyncConflictRoles::ExistingPreviewUrl)] = "existingPreviewUrl";
    result[static_cast<int>(SyncConflictRoles::ConflictPreviewUrl)] = "conflictPreviewUrl";

    return result;
}

ActivityList SyncConflictsModel::conflictActivities() const
{
    return mData;
}

void SyncConflictsModel::setConflictActivities(ActivityList conflicts)
{
    if (mData == conflicts) {
        return;
    }

    beginResetModel();

    mData = conflicts;
    emit conflictActivitiesChanged();

    updateConflictsData();

    endResetModel();
}

void SyncConflictsModel::updateConflictsData()
{
    mConflictData.clear();
    mConflictData.reserve(mData.size());

    for (const auto &oneConflict : qAsConst(mData)) {
        if (!FolderMan::instance()) {
            qCWarning(lcSyncConflictsModel) << "no FolderMan instance";
            mConflictData.push_back({});
            continue;
        }
        const auto folder = FolderMan::instance()->folder(oneConflict._folder);
        if (!folder) {
            qCWarning(lcSyncConflictsModel) << "no Folder instance for" << oneConflict._folder;
            mConflictData.push_back({});
            continue;
        }

        const auto conflictedRelativePath = oneConflict._file;
        const auto dbRecord = folder->journalDb();
        const auto baseRelativePath = dbRecord ? dbRecord->conflictFileBaseName(conflictedRelativePath.toUtf8()) : QString{};

        const auto dir = QDir(folder->path());
        const auto conflictedPath = dir.filePath(conflictedRelativePath);
        const auto basePath = dir.filePath(baseRelativePath);

        const auto existingFileInfo = QFileInfo(basePath);
        const auto conflictFileInfo = QFileInfo(conflictedPath);

        auto newConflictData = ConflictInfo{
            existingFileInfo.fileName(),
            mLocale.formattedDataSize(existingFileInfo.size()),
            mLocale.formattedDataSize(conflictFileInfo.size()),
            existingFileInfo.lastModified().toString(),
            conflictFileInfo.lastModified().toString(),
            QUrl{QStringLiteral("image://tray-image-provider/:/fileicon") + existingFileInfo.filePath()},
            QUrl{QStringLiteral("image://tray-image-provider/:/fileicon") + conflictFileInfo.filePath()},
            false,
            false,
        };

        mConflictData.push_back(std::move(newConflictData));
    }
}

}
