/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipient.h"

#include <algorithm>

#include <QJsonArray>

#include <QPointer>
#include <QJsonObject>

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

std::unique_ptr<Recipient> Recipient::fromJson(const QJsonObject &json)
{
    auto recipient = std::unique_ptr<Recipient>(new Recipient());
    recipient->updateFromJson(json);
    return recipient;
}

void Recipient::updateFromJson(const QJsonObject &json)
{
    _className = json.value("class"_L1).toString();
    _displayName = json.value("display_name"_L1).toString();
    _value = json.value("value"_L1).toString();
    if (const auto instance = json.value("instance"_L1); instance.isString()) {
        _instance = instance.toString();
    } else {
        _instance.reset();
    }

    const auto icon = json.value("icon"_L1).toObject();
    _iconSvg = icon.value("svg"_L1).toString();
    _iconLight = icon.value("light"_L1).toString();
    _iconDark = icon.value("dark"_L1).toString();

    const auto secret = json.value("secret"_L1).toObject();
    _secretUpdatable = secret.value("updatable"_L1).toBool();
    _secretValue.reset();
    _secretUrl.reset();
    if (const auto value = secret.value("value"_L1); value.isString()) {
        _secretValue = value.toString();
    }
    if (const auto url = secret.value("url"_L1); url.isString()) {
        _secretUrl = url.toString();
    }

    _initiatorDisplayName = json.value("initiator"_L1).toObject().value("display_name"_L1).toString();
    if (json.contains("permissions"_L1)) {
        qDeleteAll(_permissions);
        _permissions.clear();
        _permissionOverrides.clear();
        const auto permissions = json.value("permissions"_L1).toArray();
        for (const auto &permissionValue : permissions) {
            if (permissionValue.isObject()) {
                auto permission = Permission::fromJson(permissionValue.toObject());
                permission->setParent(this);
                _permissions.append(permission.get());
                permission.release();
            }
        }
        _hasPermissionData = true;
        Q_EMIT permissionsChanged();
    }
}

Recipient::Recipient(QObject *parent)
    : QObject{parent}
{
}

QString Recipient::className() const
{
    return _className;
}

QString Recipient::displayName() const
{
    return _displayName;
}

QString Recipient::value() const
{
    return _value;
}

const std::optional<QString> &Recipient::instance() const
{
    return _instance;
}

QString Recipient::instanceString() const
{
    return _instance.value_or(QString{});
}

QString Recipient::iconSvg() const
{
    return _iconSvg;
}

QString Recipient::iconLight() const
{
    return _iconLight;
}

QString Recipient::iconDark() const
{
    return _iconDark;
}

bool Recipient::secretUpdatable() const
{
    return _secretUpdatable;
}

const std::optional<QString> &Recipient::secretValue() const
{
    return _secretValue;
}

const std::optional<QString> &Recipient::secretUrl() const
{
    return _secretUrl;
}

QString Recipient::secretUrlString() const
{
    return _secretUrl.value_or(QString{});
}

QString Recipient::initiatorDisplayName() const
{
    return _initiatorDisplayName;
}

const QList<QPointer<Permission>> &Recipient::permissions() const
{
    return _permissions;
}

bool Recipient::hasPermissionData() const
{
    return _hasPermissionData;
}

std::optional<bool> Recipient::permissionOverride(const QString &className) const
{
    const auto it = _permissionOverrides.constFind(className);
    if (it != _permissionOverrides.cend()) {
        return it.value();
    }

    const auto permission = std::ranges::find_if(_permissions, [&className](const QPointer<Permission> &candidate) {
        return candidate && candidate->className() == className;
    });
    return permission == _permissions.cend() || !*permission ? std::nullopt : std::optional{(*permission)->enabled()};
}

void Recipient::setPermissionOverride(const QString &className, bool enabled)
{
    if (_permissionOverrides.contains(className) && _permissionOverrides.value(className) == enabled) {
        return;
    }

    _permissionOverrides.insert(className, enabled);
    for (const auto &permission : _permissions) {
        if (permission && permission->className() == className) {
            permission->setEnabled(enabled);
            break;
        }
    }
    Q_EMIT permissionsChanged();
}
