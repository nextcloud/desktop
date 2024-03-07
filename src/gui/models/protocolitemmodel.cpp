/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#include "protocolitemmodel.h"

#include "account.h"
#include "accountstate.h"
#include "models.h"
#include "gui/folderman.h"

#include "theme.h"

#include <QIcon>

using namespace OCC;

ProtocolItemModel::ProtocolItemModel(size_t size, bool issueMode, QObject *parent)
    : QAbstractTableModel(parent)
    , _data(size)
    , _issueMode(issueMode)
{
}

int ProtocolItemModel::rowCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(_data.size());
}

int ProtocolItemModel::columnCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(ProtocolItemRole::ColumnCount);
}

QVariant ProtocolItemModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));

    const auto column = static_cast<ProtocolItemRole>(index.column());
    const auto &item = protocolItem(index);
    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case ProtocolItemRole::Time:
            return item.timestamp().toLocalTime();
        case ProtocolItemRole::Folder:
            return item.folder()->shortGuiLocalPath();
        case ProtocolItemRole::Action:
            return item.message();
        case ProtocolItemRole::Size:
            return item.isSizeRelevant() ? Utility::octetsToString(item.size()) : QVariant();
        case ProtocolItemRole::File:
            return Utility::fileNameForGuiUse(item.path());
        case ProtocolItemRole::Account:
            return item.folder()->accountState()->account()->displayName();
        case ProtocolItemRole::Status:
            return Utility::enumToDisplayName(item.status());
        case ProtocolItemRole::ColumnCount:
            Q_UNREACHABLE();
            break;
        }
        break;
    case Qt::DecorationRole:
        if (column == ProtocolItemRole::Action) {
            const auto status = item.status();
            if (status == SyncFileItem::NormalError
                || status == SyncFileItem::FatalError
                || status == SyncFileItem::DetailError
                || status == SyncFileItem::BlacklistedError) {
                return Resources::themeIcon(Theme::instance()->syncStateIconName(SyncResult{SyncResult::Error}));
            } else if (Progress::isWarningKind(status) || status == SyncFileItem::Excluded) {
                return Resources::themeIcon(Theme::instance()->syncStateIconName(SyncResult{SyncResult::Problem}));
            } else {
                return Resources::themeIcon(Theme::instance()->syncStateIconName(SyncResult{SyncResult::Success}));
            }
        }
        break;
    case Models::UnderlyingDataRole:
        switch (column) {
        case ProtocolItemRole::Time:
            return item.timestamp();
        case ProtocolItemRole::Folder:
            return item.folder()->path();
        case ProtocolItemRole::Action:
            return item.message();
        case ProtocolItemRole::Size:
            return item.size();
        case ProtocolItemRole::File:
            return item.path();
        case ProtocolItemRole::Account:
            return item.folder()->accountState()->account()->displayName();
        case ProtocolItemRole::Status:
            return item.status();
        case ProtocolItemRole::ColumnCount:
            Q_UNREACHABLE();
            break;
        }
        break;
    }
    return {};
}

QVariant ProtocolItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        const auto actionRole = static_cast<ProtocolItemRole>(section);
        switch (role) {
        case Qt::DisplayRole:
            switch (actionRole) {
            case ProtocolItemRole::Time:
                return tr("Time");
            case ProtocolItemRole::File:
                return tr("File");
            case ProtocolItemRole::Folder:
                return tr("Folder");
            case ProtocolItemRole::Action:
                return _issueMode ? tr("Issues") : tr("Action");
            case ProtocolItemRole::Size:
                return tr("Size");
            case ProtocolItemRole::Account:
                return tr("Account");
            case ProtocolItemRole::Status:
                return tr("Status");
            case ProtocolItemRole::ColumnCount:
                Q_UNREACHABLE();
                break;
            }
            break;
        case Models::StringFormatWidthRole:
            switch (actionRole) {
            case ProtocolItemRole::Time:
                return 20;
            case ProtocolItemRole::Folder:
                return 30;
            case ProtocolItemRole::Action:
                return 15;
            case ProtocolItemRole::Size:
                return 6;
            case ProtocolItemRole::File:
                return 64;
            case ProtocolItemRole::Account:
                return 20;
            case ProtocolItemRole::Status:
                return 20;
            case ProtocolItemRole::ColumnCount:
                Q_UNREACHABLE();
                break;
            }
            break;
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ProtocolItemModel::addProtocolItem(ProtocolItem &&item)
{
    if (_data.isFull()) {
        beginRemoveRows(QModelIndex(), 0, 0);
        _data.pop_front();
        endRemoveRows();
    }
    const auto size = static_cast<int>(_data.size());
    beginInsertRows(QModelIndex(), size, size);
    _data.push_back(std::move(item));
    endInsertRows();
}

const ProtocolItem &ProtocolItemModel::protocolItem(const QModelIndex &index) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    return _data.at(index.row());
}

void ProtocolItemModel::reset(std::vector<ProtocolItem> &&data)
{
    beginResetModel();
    _data.reset(std::move(data));
    endResetModel();
}

void ProtocolItemModel::remove_if(const std::function<bool(const ProtocolItem &)> &filter)
{
    if (_data.empty()) {
        return;
    }
    beginResetModel();
    _data.remove_if(filter);
    endResetModel();
}
