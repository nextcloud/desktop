/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "permissionmodel.h"

#include <QPointer>
#include <qnamespace.h>

#include "share.h"
#include "permission.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

PermissionModel::PermissionModel(QObject *parent)
    : QAbstractListModel{parent}
{}

int PermissionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !_share) {
        return 0;
    }

    qCritical() << "permissions size:" << _share->permissions().size();
    return _share->permissions().size();
}

QVariant PermissionModel::data(const QModelIndex &index, int role) const
{
    if (!_share) {
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
    qCritical() << "setData called with" << index << value << role;

    if (role != EnabledRole) {
        return false;
    }

    const auto newValue = value.toBool();

    Q_EMIT dataChanged(index, index, {EnabledRole});
    return true;
}

Qt::ItemFlags PermissionModel::flags(const QModelIndex &index) const
{
    qCritical() << "flags for" << index << QAbstractListModel::flags(index) << Qt::ItemIsEditable << "returning" << (QAbstractListModel::flags(index) | Qt::ItemIsEditable);
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
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

Share *PermissionModel::share() const
{
    return _share;
}

void PermissionModel::setShare(Share *share)
{
    if (_share == share) {
        return;
    }

    beginResetModel();
    _share = share;
    connect(_share, &Share::permissionsChanged, this, [this]() -> void {
        beginResetModel();
        endResetModel();
    });
    Q_EMIT shareChanged();
    endResetModel();
}
