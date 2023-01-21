/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#include "sharee.h"
#include "ocsshareejob.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace OCC {

Q_LOGGING_CATEGORY(lcSharing, "nextcloud.gui.sharing", QtInfoMsg)

Sharee::Sharee(const QString shareWith,
    const QString displayName,
    const Type type)
    : _shareWith(shareWith)
    , _displayName(displayName)
    , _type(type)
{
}

QString Sharee::format() const
{
    QString formatted = _displayName;

    if (_type == Type::Group) {
        formatted += QLatin1String(" (group)");
    } else if (_type == Type::Email) {
        formatted += QLatin1String(" (email)");
    } else if (_type == Type::Federated) {
        formatted += QLatin1String(" (remote)");
    } else if (_type == Type::Circle) {
        formatted += QLatin1String(" (circle)");
    } else if (_type == Type::Room) {
        formatted += QLatin1String(" (conversation)");
    }

    return formatted;
}

QString Sharee::shareWith() const
{
    return _shareWith;
}

QString Sharee::displayName() const
{
    return _displayName;
}

Sharee::Type Sharee::type() const
{
    return _type;
}

}
