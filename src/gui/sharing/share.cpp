
/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "share.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QLoggingCategory>

#include "property.h"

Q_LOGGING_CATEGORY(lcSharingShare, "nextcloud.gui.sharing.share", QtInfoMsg)

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

QPointer<Share> Share::fromJson(const QJsonDocument &json, const AccountPtr &account)
{
    auto share = QPointer<Share>{new Share(account)};
    share->updateFromJson(json);
    return share;
}

void Share::updateFromJson(const QJsonDocument &json)
{
    qCDebug(lcSharingShare) << "updating share from json" << json;
    const auto data = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
    setId(data.value("id"_L1).toString());
    setState(data.value("state"_L1).toString());
    setPermissions(data.value("permissions"_L1).toArray());
    setProperties(data.value("properties"_L1).toArray());
}

Share::Share(const AccountPtr &account)
    :_account{account}
{}

QString Share::id() const
{
    return _id;
}

Share::ShareState Share::state() const
{
    return _state;
}

const QList<QPointer<Permission>> &Share::permissions() const
{
    return _permissions;
}

const QList<QPointer<Property>> &Share::properties() const
{
    return _properties;
}

void Share::setId(const QString &id)
{
    if (_id == id) {
        return;
    }

    _id = id;
    Q_EMIT idChanged();
}

void Share::setState(const QString &state)
{
    ShareState newState = ShareState::Draft;

    if (state == "active"_L1) {
        newState = ShareState::Active;
    } else if (state == "deleted"_L1) {
        newState = ShareState::Deleted;
    }

    if (_state == newState) {
        return;
    }

    _state = newState;
    Q_EMIT stateChanged();
}

void Share::setPermissions(const QJsonArray &permissions)
{
    _permissions.clear();

    if (permissions.isEmpty()) {
        Q_EMIT permissionsChanged();
        return;
    }

    for (const auto &permissionValue : permissions) {
        if (!permissionValue.isObject()) {
            continue;
        }
        const auto permissionObject = permissionValue.toObject();
        _permissions.append(Permission::fromJson(permissionObject));
    }

    Q_EMIT permissionsChanged();
}

void Share::setProperties(const QJsonArray &properties)
{
    _properties.clear();

    if (properties.isEmpty()) {
        Q_EMIT propertiesChanged();
        return;
    }

    for (const auto &propertyValue : properties) {
        if (!propertyValue.isObject()) {
            continue;
        }
        const auto propertyObject = propertyValue.toObject();
        _properties.append(Property::fromJson(propertyObject));
    }

    Q_EMIT propertiesChanged();
}
