/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipient.h"

#include <QPointer>
#include <QJsonObject>

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

QPointer<Recipient> Recipient::fromJson(const QJsonObject &json)
{
    auto recipient = QPointer<Recipient>(new Recipient);
    recipient->_className = json.value("class"_L1).toString();
    recipient->_displayName = json.value("display_name"_L1).toString();
    recipient->_value = json.value("value"_L1).toString();
    if (const auto instance = json.value("instance"_L1); instance.isString()) {
        recipient->_instance = instance.toString();
    }

    const auto icon = json.value("icon"_L1).toObject();
    recipient->_iconSvg = icon.value("svg"_L1).toString();
    recipient->_iconLight = icon.value("light"_L1).toString();
    recipient->_iconDark = icon.value("dark"_L1).toString();

    const auto secret = json.value("secret"_L1).toObject();
    recipient->_secretUpdatable = secret.value("updatable"_L1).toBool();
    if (const auto value = secret.value("value"_L1); value.isString()) {
        recipient->_secretValue = value.toString();
    }
    if (const auto url = secret.value("url"_L1); url.isString()) {
        recipient->_secretUrl = url.toString();
    }

    recipient->_initiatorDisplayName = json.value("initiator"_L1).toObject().value("display_name"_L1).toString();
    return recipient;
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
