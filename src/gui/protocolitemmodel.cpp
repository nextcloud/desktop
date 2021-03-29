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

namespace {
auto getFolder(const OCC::ProtocolItem &item)
{
    auto f = OCC::FolderMan::instance()->folder(item.folderName());
    OC_ASSERT(f);
    return f;
}
}

using namespace OCC;

ProtocolItemModel::ProtocolItemModel(QObject *parent, bool issueMode, int maxLogSize)
    : QAbstractTableModel(parent)
    , _issueMode(issueMode)
    , _maxLogSize(maxLogSize)
{
    _data.reserve(maxLogSize);
}

int ProtocolItemModel::rowCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid()) {
        return 0;
    }
    return actualSize();
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
            return item.timestamp();
        case ProtocolItemRole::Folder:
            return getFolder(item)->shortGuiLocalPath();
        case ProtocolItemRole::Action:
            return item.message();
        case ProtocolItemRole::Size:
            return item.isSizeRelevant() ? Utility::octetsToString(item.size()) : QVariant();
        case ProtocolItemRole::File:
            return Utility::fileNameForGuiUse(item.path());
        case ProtocolItemRole::Account:
            return getFolder(item)->accountState()->account()->displayName();
        case ProtocolItemRole::ColumnCount:
            Q_UNREACHABLE();
            break;
        }
    case Qt::DecorationRole:
        if (column == ProtocolItemRole::Action) {
            const auto status = item.status();
            if (status == SyncFileItem::NormalError
                || status == SyncFileItem::FatalError
                || status == SyncFileItem::DetailError
                || status == SyncFileItem::BlacklistedError) {
                return Theme::instance()->syncStateIcon(SyncResult::Error);
            } else if (Progress::isWarningKind(status)) {
                return Theme::instance()->syncStateIcon(SyncResult::Problem);
            } else {
                return {};
                // TODO: display icon on success?
                //                return Theme::instance()->syncStateIcon(SyncResult::Success);
            }
        }
    case Models::UnderlyingDataRole:
        switch (column) {
        case ProtocolItemRole::Time:
            return item.timestamp();
        case ProtocolItemRole::Folder:
            return item.folderName();
        case ProtocolItemRole::Action:
            return item.message();
        case ProtocolItemRole::Size:
            return item.size();
        case ProtocolItemRole::File:
            return item.path();
        case ProtocolItemRole::Account:
            return getFolder(item)->accountState()->account()->displayName();
        case ProtocolItemRole::ColumnCount:
            Q_UNREACHABLE();
            break;
        }
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
            case ProtocolItemRole::ColumnCount:
                Q_UNREACHABLE();
                break;
            };

        case Models::StringFormatWidthRole:
            // TODO: fine tune
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
            case ProtocolItemRole::ColumnCount:
                Q_UNREACHABLE();
                break;
            }
        };
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ProtocolItemModel::addProtocolItem(const ProtocolItem &&item)
{
    Q_ASSERT(actualSize() == _data.size());
    if (_data.size() >= _maxLogSize) {
        beginRemoveRows(QModelIndex(), 0, 0);
        _start++;
        endRemoveRows();
    } else {
        _data.push_back({});
    }
    // _data.size() might differ
    const auto size = actualSize();
    beginInsertRows(QModelIndex(), size, size);
    _data[convertToIndex(size)] = std::move(item);
    _end++;
    endInsertRows();
}

const ProtocolItem &ProtocolItemModel::protocolItem(const QModelIndex &index) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    return _data.at(convertToIndex(index.row()));
}

const std::vector<ProtocolItem> &ProtocolItemModel::rawData() const
{
    return _data;
}

void ProtocolItemModel::remove(const std::function<bool(const ProtocolItem &)> &filter)
{
    if (_data.empty()) {
        return;
    }
    const auto first = protocolItem(index(0, 0));
    beginResetModel();
    _data.erase(std::remove_if(_data.begin(), _data.end(), filter), _data.end());
    // find start again
    _start = std::distance(_data.cbegin(), std::find_if(_data.cbegin(), _data.cend(), [&first](const ProtocolItem &pi) {
        return pi._id <= first._id;
    }));
    _end = _start + _data.size();
    endResetModel();
    _data.reserve(_maxLogSize);
}
