/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "permissionmodel.h"

#include "permission.h"
#include "recipient.h"
#include "unifiedshare.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

PermissionModel::PermissionModel(QObject *parent)
    : ShareDetailsListModel{parent}
{}

int PermissionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || (!_share && !_recipient)) {
        return 0;
    }

    const auto useRecipientPermissions = !_share && _recipient;
    if (useRecipientPermissions) {
        return _recipient->permissions().size();
    }
    return _share ? _share->permissions().size() : 0;
}

QVariant PermissionModel::data(const QModelIndex &index, int role) const
{
    if ((!_share && !_recipient) || !checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto useRecipientPermissions = !_share && _recipient;
    const auto &permissions = useRecipientPermissions ? _recipient->permissions() : _share->permissions();
    const auto permission = permissions.at(index.row());

    switch (role) {
    case LabelRole:
        return permission->displayName();
    case ClassNameRole:
        return permission->className();
    case PlaceholderRole:
        return permission->hint();
    case EnabledRole:
        if (_recipient && _share) {
            return _recipient->permissionOverride(permission->className()).value_or(permission->enabled());
        }
        return permission->enabled();
    case AvailableRole:
        return !_recipient || !_share || permission->enabled();
    default:
        return {};
    }
}

QHash<int, QByteArray> PermissionModel::roleNames() const
{
    return {
        {LabelRole, "label"_ba},
        {ClassNameRole, "className"_ba},
        {PlaceholderRole, "hint"_ba},
        {EnabledRole, "enabled"_ba},
        {AvailableRole, "available"_ba},
    };
};

void PermissionModel::setShare(Share *share)
{
    if (_share == share) {
        return;
    }

    QObject::disconnect(_permissionsChangedConnection);
    ShareDetailsListModel::setShare(share);
    if (!_share) {
        return;
    }

    _permissionsChangedConnection = connect(_share, &Share::permissionsChanged, this, [this]() -> void {
        beginResetModel();
        endResetModel();
    });
}

Recipient *PermissionModel::recipient() const
{
    return _recipient;
}

void PermissionModel::setRecipient(Recipient *recipient)
{
    if (_recipient == recipient) {
        return;
    }

    QObject::disconnect(_recipientPermissionsChangedConnection);
    beginResetModel();
    _recipient = recipient;
    Q_EMIT recipientChanged();
    endResetModel();

    if (_recipient) {
        _recipientPermissionsChangedConnection = connect(_recipient, &Recipient::permissionsChanged, this, [this]() -> void {
            beginResetModel();
            endResetModel();
        });
    }
}
