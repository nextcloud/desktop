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

    return _data.size();
}

QVariant SyncConflictsModel::data(const QModelIndex &index, int role) const
{
    auto result = QVariant{};

    Q_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid));

    if (index.parent().isValid()) {
        return result;
    }

    if (role >= static_cast<int>(SyncConflictRoles::ExistingFileName) && role <= static_cast<int>(SyncConflictRoles::ConflictPreviewUrl)) {
        const auto convertedRole = static_cast<SyncConflictRoles>(role);

        switch (convertedRole) {
        case SyncConflictRoles::ExistingFileName:
            result = _conflictData[index.row()].mExistingFileName;
            break;
        case SyncConflictRoles::ExistingSize:
            result = _conflictData[index.row()].mExistingSize;
            break;
        case SyncConflictRoles::ConflictSize:
            result = _conflictData[index.row()].mConflictSize;
            break;
        case SyncConflictRoles::ExistingDate:
            result = _conflictData[index.row()].mExistingDate;
            break;
        case SyncConflictRoles::ConflictDate:
            result = _conflictData[index.row()].mConflictDate;
            break;
        case SyncConflictRoles::ExistingSelected:
            result = _conflictData[index.row()].mExistingSelected == ConflictInfo::ConflictSolution::SolutionSelected;
            break;
        case SyncConflictRoles::ConflictSelected:
            result = _conflictData[index.row()].mConflictSelected == ConflictInfo::ConflictSolution::SolutionSelected;
            break;
        case SyncConflictRoles::ExistingPreviewUrl:
            result = _conflictData[index.row()].mExistingPreviewUrl;
            break;
        case SyncConflictRoles::ConflictPreviewUrl:
            result = _conflictData[index.row()].mConflictPreviewUrl;
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
        const auto convertedRole = static_cast<SyncConflictRoles>(role);

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
    return _data;
}

bool SyncConflictsModel::allExistingsSelected() const
{
    return _allExistingsSelected;
}

bool SyncConflictsModel::allConflictingSelected() const
{
    return _allConflictingsSelected;
}

void SyncConflictsModel::setConflictActivities(ActivityList conflicts)
{
    if (_data == conflicts) {
        return;
    }

    beginResetModel();

    _data = conflicts;
    emit conflictActivitiesChanged();

    updateConflictsData();

    endResetModel();
}

void SyncConflictsModel::selectAllExisting(bool selected)
{
    for (auto &singleConflict : _conflictData) {
        singleConflict.mExistingSelected = selected ? ConflictInfo::ConflictSolution::SolutionSelected : ConflictInfo::ConflictSolution::SolutionDeselected;
    }

    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {static_cast<int>(SyncConflictRoles::ExistingSelected)});

    if (selected && !_allExistingsSelected) {
        _allExistingsSelected = true;
        Q_EMIT allExistingsSelectedChanged();
    } else if (!selected && _allExistingsSelected) {
        _allExistingsSelected = false;
        Q_EMIT allExistingsSelectedChanged();
    }
}

void SyncConflictsModel::selectAllConflicting(bool selected)
{
    for (auto &singleConflict : _conflictData) {
        singleConflict.mConflictSelected = selected ? ConflictInfo::ConflictSolution::SolutionSelected : ConflictInfo::ConflictSolution::SolutionDeselected;
    }

    Q_EMIT dataChanged(index(0), index(rowCount() - 1), {static_cast<int>(SyncConflictRoles::ConflictSelected)});

    if (selected && !_allConflictingsSelected) {
        _allConflictingsSelected = true;
        Q_EMIT allConflictingSelectedChanged();
    } else if (!selected && _allConflictingsSelected) {
        _allConflictingsSelected = false;
        Q_EMIT allConflictingSelectedChanged();
    }
}

void SyncConflictsModel::applySolution()
{
    for(const auto &syncConflict : std::as_const(_conflictData)) {
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
    _conflictData.clear();
    _conflictData.reserve(_data.size());

    for (const auto &oneConflict : std::as_const(_data)) {
        const auto folder = FolderMan::instance()->folder(oneConflict._folder);
        if (!folder) {
            qCWarning(lcSyncConflictsModel) << "no Folder instance for" << oneConflict._folder;
            _conflictData.push_back({});
            continue;
        }

        const auto conflictedRelativePath = oneConflict._file;
        const auto baseRelativePath = folder->journalDb() ? folder->journalDb()->conflictFileBaseName(conflictedRelativePath.toUtf8()) : QString{};

        const auto dir = QDir(folder->path());
        const auto conflictedPath = dir.filePath(conflictedRelativePath);
        const auto basePath = dir.filePath(baseRelativePath);

        const auto existingFileInfo = QFileInfo(basePath);
        const auto conflictFileInfo = QFileInfo(conflictedPath);

        auto newConflictData = ConflictInfo{
            existingFileInfo.fileName(),
            _locale.formattedDataSize(existingFileInfo.size()),
            _locale.formattedDataSize(conflictFileInfo.size()),
            existingFileInfo.lastModified().toString(),
            conflictFileInfo.lastModified().toString(),
            QUrl{QStringLiteral("image://tray-image-provider/:/fileicon") + existingFileInfo.filePath()},
            QUrl{QStringLiteral("image://tray-image-provider/:/fileicon") + conflictFileInfo.filePath()},
            ConflictInfo::ConflictSolution::SolutionDeselected,
            ConflictInfo::ConflictSolution::SolutionDeselected,
            existingFileInfo.filePath(),
            conflictFileInfo.filePath(),
        };

        _conflictData.push_back(std::move(newConflictData));
    }
}

void SyncConflictsModel::setExistingSelected(bool value,
                                             const QModelIndex &index,
                                             int role)
{
    _conflictData[index.row()].mExistingSelected = value ? ConflictInfo::ConflictSolution::SolutionSelected : ConflictInfo::ConflictSolution::SolutionDeselected;
    Q_EMIT dataChanged(index, index, {role});

    if (_conflictData[index.row()].mExistingSelected == ConflictInfo::ConflictSolution::SolutionDeselected && _allExistingsSelected) {
        _allExistingsSelected = false;
        Q_EMIT allExistingsSelectedChanged();
    } else if (_conflictData[index.row()].mExistingSelected == ConflictInfo::ConflictSolution::SolutionSelected && !_allExistingsSelected) {
        const auto deselectedConflictIt = std::find_if(_conflictData.constBegin(), _conflictData.constEnd(), [] (const auto singleConflict) {
            return singleConflict.mExistingSelected == ConflictInfo::ConflictSolution::SolutionDeselected;
        });
        const auto allSelected = (deselectedConflictIt == _conflictData.constEnd());
        if (allSelected) {
            _allExistingsSelected = true;
            Q_EMIT allExistingsSelectedChanged();
        }
    }
}

void SyncConflictsModel::setConflictingSelected(bool value,
                                                const QModelIndex &index,
                                                int role)
{
    _conflictData[index.row()].mConflictSelected = value ? ConflictInfo::ConflictSolution::SolutionSelected : ConflictInfo::ConflictSolution::SolutionDeselected;
    Q_EMIT dataChanged(index, index, {role});

    if (_conflictData[index.row()].mConflictSelected == ConflictInfo::ConflictSolution::SolutionDeselected && _allConflictingsSelected) {
        _allConflictingsSelected = false;
        Q_EMIT allConflictingSelectedChanged();
    } else if (_conflictData[index.row()].mConflictSelected == ConflictInfo::ConflictSolution::SolutionSelected && !_allConflictingsSelected) {
        const auto deselectedConflictIt = std::find_if(_conflictData.constBegin(), _conflictData.constEnd(), [] (const auto singleConflict) {
            return singleConflict.mConflictSelected == ConflictInfo::ConflictSolution::SolutionDeselected;
        });
        const auto allSelected = (deselectedConflictIt == _conflictData.constEnd());
        if (allSelected) {
            _allConflictingsSelected = true;
            Q_EMIT allConflictingSelectedChanged();
        }
    }
}

ConflictSolver::Solution SyncConflictsModel::ConflictInfo::solution() const
{
    auto result = ConflictSolver::Solution{};

    if (mConflictSelected == ConflictSolution::SolutionSelected && mExistingSelected == ConflictSolution::SolutionSelected) {
        result = ConflictSolver::KeepBothVersions;
    } else if (mConflictSelected == ConflictSolution::SolutionDeselected && mExistingSelected == ConflictSolution::SolutionSelected) {
        result = ConflictSolver::KeepRemoteVersion;
    } else if (mConflictSelected == ConflictSolution::SolutionSelected && mExistingSelected == ConflictSolution::SolutionDeselected) {
        result = ConflictSolver::KeepLocalVersion;
    }

    return result;
}

bool SyncConflictsModel::ConflictInfo::isValid() const
{
    return mConflictSelected == ConflictInfo::ConflictSolution::SolutionSelected || mExistingSelected == ConflictInfo::ConflictSolution::SolutionSelected;
}

}
