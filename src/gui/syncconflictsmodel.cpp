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

bool SyncConflictsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    auto result = false;

    Q_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid));

    if (index.parent().isValid()) {
        return result;
    }

    if (role >= static_cast<int>(SyncConflictRoles::ExistingFileName) && role <= static_cast<int>(SyncConflictRoles::ConflictPreviewUrl)) {
        auto convertedRole = static_cast<SyncConflictRoles>(role);

        switch(convertedRole) {
        case SyncConflictRoles::ExistingFileName:
            break;
        case SyncConflictRoles::ExistingSize:
            break;
        case SyncConflictRoles::ConflictSize:
            break;
        case SyncConflictRoles::ExistingDate:
            break;
        case SyncConflictRoles::ConflictDate:
            break;
        case SyncConflictRoles::ExistingSelected:
            setExistingSelected(value.toBool(), index, role);
            result = true;
            break;
        case SyncConflictRoles::ConflictSelected:
            setConflictingSelected(value.toBool(), index, role);
            result = true;
            break;
        case SyncConflictRoles::ExistingPreviewUrl:
            break;
        case SyncConflictRoles::ConflictPreviewUrl:
            break;
        }

        result = false;
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

Qt::ItemFlags SyncConflictsModel::flags(const QModelIndex &index) const
{
    auto result = Qt::ItemFlags{};

    if (!index.parent().isValid()) {
        result = QAbstractListModel::flags(index);
        return result;
    }

    result = Qt::ItemIsSelectable | Qt::ItemIsEditable;
    return result;
}

ActivityList SyncConflictsModel::conflictActivities() const
{
    return mData;
}

bool SyncConflictsModel::allExistingsSelected() const
{
    return mAllExistingsSelected;
}

bool SyncConflictsModel::allConflictingSelected() const
{
    return mAllConflictingsSelected;
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

void SyncConflictsModel::selectAllExisting(bool selected)
{
    for (auto &singleConflict : mConflictData) {
        singleConflict.mExistingSelected = selected;
    }

    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {static_cast<int>(SyncConflictRoles::ExistingSelected)});

    if (selected && !mAllExistingsSelected) {
        mAllExistingsSelected = true;
        Q_EMIT allExistingsSelectedChanged();
    } else if (!selected && mAllExistingsSelected) {
        mAllExistingsSelected = false;
        Q_EMIT allExistingsSelectedChanged();
    }
}

void SyncConflictsModel::selectAllConflicting(bool selected)
{
    for (auto &singleConflict : mConflictData) {
        singleConflict.mConflictSelected = selected;
    }

    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {static_cast<int>(SyncConflictRoles::ConflictSelected)});

    if (selected && !mAllConflictingsSelected) {
        mAllConflictingsSelected = true;
        Q_EMIT allConflictingSelectedChanged();
    } else if (!selected && mAllConflictingsSelected) {
        mAllConflictingsSelected = false;
        Q_EMIT allConflictingSelectedChanged();
    }
}

void SyncConflictsModel::applyResolution()
{
    for(const auto &syncConflict : qAsConst(mConflictData)) {
        if (syncConflict.isValid()) {
            qCInfo(lcSyncConflictsModel) << syncConflict.mExistingFilePath << syncConflict.mConflictingFilePath << syncConflict.solution();
            ConflictSolver solver;
            solver.setLocalVersionFilename(syncConflict.mConflictingFilePath);
            solver.setRemoteVersionFilename(syncConflict.mExistingFilePath);
            solver.exec(syncConflict.solution());
        }
    }
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
            existingFileInfo.filePath(),
            conflictFileInfo.filePath(),
        };

        mConflictData.push_back(std::move(newConflictData));
    }
}

void SyncConflictsModel::setExistingSelected(bool value,
                                             const QModelIndex &index,
                                             int role)
{
    mConflictData[index.row()].mExistingSelected = value;
    Q_EMIT dataChanged(index, index, {role});

    if (!mConflictData[index.row()].mExistingSelected && mAllExistingsSelected) {
        mAllExistingsSelected = false;
        Q_EMIT allExistingsSelectedChanged();
    } else if (mConflictData[index.row()].mExistingSelected && !mAllExistingsSelected) {
        auto allSelected = true;
        for (const auto &singleConflict : qAsConst(mConflictData)) {
            if (!singleConflict.mExistingSelected) {
                allSelected = false;
                break;
            }
        }
        if (allSelected) {
            mAllExistingsSelected = true;
            Q_EMIT allExistingsSelectedChanged();
        }
    }
}

void SyncConflictsModel::setConflictingSelected(bool value,
                                                const QModelIndex &index,
                                                int role)
{
    mConflictData[index.row()].mConflictSelected = value;
    Q_EMIT dataChanged(index, index, {role});

    if (!mConflictData[index.row()].mConflictSelected && mAllConflictingsSelected) {
        mAllConflictingsSelected = false;
        Q_EMIT allConflictingSelectedChanged();
    } else if (mConflictData[index.row()].mConflictSelected && !mAllConflictingsSelected) {
        auto allSelected = true;
        for (const auto &singleConflict : qAsConst(mConflictData)) {
            if (!singleConflict.mConflictSelected) {
                allSelected = false;
                break;
            }
        }
        if (allSelected) {
            mAllConflictingsSelected = true;
            Q_EMIT allConflictingSelectedChanged();
        }
    }
}

ConflictSolver::Solution SyncConflictsModel::ConflictInfo::solution() const
{
    auto result = ConflictSolver::Solution{};

    if (mConflictSelected && mExistingSelected) {
        result = ConflictSolver::KeepBothVersions;
    } else if (!mConflictSelected && mExistingSelected) {
        result = ConflictSolver::KeepLocalVersion;
    } else if (mConflictSelected && !mExistingSelected) {
        result = ConflictSolver::KeepRemoteVersion;
    }

    return result;
}

bool SyncConflictsModel::ConflictInfo::isValid() const
{
    return mConflictSelected || mExistingSelected;
}

}
