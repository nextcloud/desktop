/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "permissionmodel.h"

#include <QPointer>

#include "share.h"
#include "permission.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

PermissionModel::PermissionModel(QObject *parent)
    : AbstractShareModel{parent}
{}

int PermissionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !_share) {
        return 0;
    }

    return _share->permissions().size();
}

QVariant PermissionModel::data(const QModelIndex &index, int role) const
{
    if (!_share || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto permissions = _share->permissions();
    const auto permission = permissions.at(index.row());

    switch (role) {
    case LabelRole:
        return permission->displayName();
    case ClassNameRole:
        return permission->className();
    case PlaceholderRole:
        return permission->hint();
    case EnabledRole:
        return permission->enabled();
    default:
        return {};
    }
}

bool PermissionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(value)
    if (role != EnabledRole || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    Q_EMIT dataChanged(index, index, {EnabledRole});
    return true;
}

Qt::ItemFlags PermissionModel::flags(const QModelIndex &index) const
{
    return checkIndex(index, CheckIndexOption::IndexIsValid) ? QAbstractListModel::flags(index) | Qt::ItemIsEditable : Qt::NoItemFlags;
}

QHash<int, QByteArray> PermissionModel::roleNames() const
{
    return {
        { LabelRole, "label"_ba},
        { ClassNameRole, "className"_ba},
        { PlaceholderRole, "hint"_ba},
        { EnabledRole, "enabled"_ba},
    };
};

void PermissionModel::setShare(Share *share)
{
    AbstractShareModel::setShare(share);
    if (!_share) {
        return;
    }

    connect(_share, &Share::permissionsChanged, this, [this]() -> void {
        beginResetModel();
        endResetModel();
    });
}
