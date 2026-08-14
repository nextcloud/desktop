
/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "share.h"

#include "sharingconstants.h"

#include <algorithm>

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
    if (data.contains("id"_L1)) {
        setId(data.value("id"_L1).toString());
    }
    if (data.contains("state"_L1)) {
        setState(data.value("state"_L1).toString());
    }
    if (data.contains("permission_preset"_L1)) {
        setPermissionPreset(data.value("permission_preset"_L1).toString());
    }
    if (data.contains("permissions"_L1)) {
        setPermissions(data.value("permissions"_L1).toArray());
    }
    if (data.contains("properties"_L1)) {
        setProperties(data.value("properties"_L1).toArray());
    }
    if (data.contains("recipients"_L1)) {
        setRecipients(data.value("recipients"_L1).toArray());
    }
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

QString Share::permissionPreset() const
{
    return _permissionPreset;
}

const QList<QPointer<Permission>> &Share::permissions() const
{
    return _permissions;
}

const QList<QPointer<Property>> &Share::properties() const
{
    return _properties;
}

const QList<QPointer<Recipient>> &Share::recipients() const
{
    return _recipients;
}

bool Share::isPublicLink() const
{
    return std::ranges::any_of(_recipients, [](const QPointer<Recipient> &recipient) {
        return recipient && recipient->className() == RecipientTypeClasses::token;
    });
}

QString Share::publicLinkUrl() const
{
    const auto recipient = std::ranges::find_if(_recipients, [](const QPointer<Recipient> &recipient) {
        return recipient && recipient->className() == RecipientTypeClasses::token;
    });
    return recipient == _recipients.cend() || !*recipient ? QString{} : (*recipient)->secretUrlString();
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
    auto newState = ShareState::Unknown;

    if (state == "draft"_L1) {
        newState = ShareState::Draft;
    } else if (state == "active"_L1) {
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

void Share::setPermissionPreset(const QString &permissionPreset)
{
    if (_permissionPreset == permissionPreset) {
        return;
    }

    _permissionPreset = permissionPreset;
    Q_EMIT permissionPresetChanged();
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

void Share::setRecipients(const QJsonArray &recipients)
{
    _recipients.clear();

    if (recipients.isEmpty()) {
        Q_EMIT recipientsChanged();
        return;
    }

    for (const auto &recipientValue : recipients) {
        if (!recipientValue.isObject()) {
            continue;
        }
        const auto recipientObject = recipientValue.toObject();
        _recipients.append(Recipient::fromJson(recipientObject));
    }

    Q_EMIT recipientsChanged();
}
