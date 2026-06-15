
/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "permission.h"

#include <QPointer>
#include <QJsonObject>

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

QPointer<Permission> Permission::fromJson(const QJsonObject &json)
{
    const auto className = json.value("class"_L1).toString();
    const auto displayName = json.value("display_name"_L1).toString();
    const auto enabled = json.value("enabled"_L1).toBool();
    const auto hint = json.value("hint"_L1).toString();

    auto permission = QPointer<Permission>(new Permission {
        className,
        displayName,
        enabled,
        hint,
    });
    return permission;
}

Permission::Permission(const QString &className, const QString &displayName, bool enabled, const QString &hint, QObject *parent)
    : QObject{parent}
    , _className{className}
    , _displayName{displayName}
    , _enabled{enabled}
    , _hint{hint}
{
}

QString Permission::className() const
{
    return _className;
}

QString Permission::displayName() const
{
    return _displayName;
}

bool Permission::enabled() const
{
    return _enabled;
}

QString Permission::hint() const
{
    return _hint;
}

void Permission::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    Q_EMIT enabledChanged();
}
