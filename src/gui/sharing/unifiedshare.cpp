
/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedshare.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>
#include <utility>

#include "property.h"
#include "sharingconstants.h"


Q_LOGGING_CATEGORY(lcSharingShare, "nextcloud.gui.sharing.share", QtInfoMsg)

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

std::unique_ptr<Share> Share::fromJson(const QJsonDocument &json, const AccountPtr &account)
{
    auto share = std::unique_ptr<Share>(new Share(account));
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

Share::State Share::state() const
{
    return _state;
}

QString Share::permissionPreset() const
{
    return _permissionPreset;
}

QString Share::permissionPresetLabel() const
{
    if (_permissionPreset.endsWith("\\ViewSharePermissionPreset"_L1)) {
        return tr("View only");
    }
    if (_permissionPreset.endsWith("\\EditSharePermissionPreset"_L1)) {
        return tr("Can edit");
    }
    return {};
}

QList<Permission *> Share::permissions() const
{
    auto permissions = QList<Permission *>{};
    permissions.reserve(static_cast<qsizetype>(_permissions.size()));
    for (const auto &permission : _permissions) {
        permissions.append(permission.get());
    }
    return permissions;
}

QList<Property *> Share::properties() const
{
    auto properties = QList<Property *>{};
    properties.reserve(static_cast<qsizetype>(_properties.size()));
    for (const auto &property : _properties) {
        properties.append(property.get());
    }
    return properties;
}

QList<Recipient *> Share::recipients() const
{
    auto recipients = QList<Recipient *>{};
    recipients.reserve(static_cast<qsizetype>(_recipients.size()));
    for (const auto &recipient : _recipients) {
        recipients.append(recipient.get());
    }
    return recipients;
}

bool Share::isPublicLink() const
{
    return std::ranges::any_of(_recipients, [](const auto &recipient) {
        return recipient && recipient->className() == RecipientTypeClasses::token;
    });
}

QString Share::publicLinkUrl() const
{
    const auto recipient = std::ranges::find_if(_recipients, [](const auto &recipient) {
        return recipient && recipient->className() == RecipientTypeClasses::token;
    });
    return recipient == _recipients.cend() ? QString{} : (*recipient)->secretUrlString();
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
    auto newState = State::Unknown;

    if (state == "draft"_L1) {
        newState = State::Draft;
    } else if (state == "active"_L1) {
        newState = State::Active;
    } else if (state == "deleted"_L1) {
        newState = State::Deleted;
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
        auto permission = Permission::fromJson(permissionObject);
        _permissions.emplace_back(std::move(permission));
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
        auto property = Property::fromJson(propertyObject);
        _properties.emplace_back(std::move(property));
    }

    Q_EMIT propertiesChanged();
}

void Share::setRecipients(const QJsonArray &recipients)
{
    auto updatedRecipients = std::vector<std::unique_ptr<Recipient>>{};
    updatedRecipients.reserve(static_cast<size_t>(recipients.size()));
    for (const auto &recipientValue : recipients) {
        if (!recipientValue.isObject()) {
            continue;
        }
        const auto recipientObject = recipientValue.toObject();
        const auto className = recipientObject.value("class"_L1).toString();
        const auto value = recipientObject.value("value"_L1).toString();
        const auto instance = recipientObject.value("instance"_L1).toString();
        const auto existing = std::ranges::find_if(_recipients, [&className, &value, &instance](const auto &recipient) {
            return recipient && recipient->className() == className && recipient->value() == value && recipient->instanceString() == instance;
        });
        if (existing != _recipients.cend()) {
            (*existing)->updateFromJson(recipientObject);
            updatedRecipients.emplace_back(std::move(*existing));
        } else {
            auto recipient = Recipient::fromJson(recipientObject);
            updatedRecipients.emplace_back(std::move(recipient));
        }
    }
    _recipients = std::move(updatedRecipients);

    Q_EMIT recipientsChanged();
}
