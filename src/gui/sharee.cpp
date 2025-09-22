/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharee.h"
#include "ocsshareejob.h"
#include "theme.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace OCC
{
Q_LOGGING_CATEGORY(lcSharing, "nextcloud.gui.sharing", QtInfoMsg)

Sharee::Sharee(const QString &shareWith, const QString &displayName, const Type type, const QString &iconUrl)
    : _shareWith(shareWith)
    , _displayName(displayName)
    , _type(type)
    , _iconUrl(iconUrl)
{
    if (!_iconUrl.isEmpty()) {
        // make sure no color path is contained in the url
        _iconUrl.replace(QStringLiteral("/black"), "");
        _iconUrl.replace(QStringLiteral("/white"), "");
        _iconColor = Theme::instance()->darkMode() ? QStringLiteral("white") : QStringLiteral("black");
    }
    updateIconUrl();
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
    } else if (_type == Type::Team) {
        formatted += QLatin1String(" (team)");
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

void Sharee::setDisplayName(const QString &displayName)
{
    if (displayName != _displayName) {
        _displayName = displayName;
    }
}

void Sharee::setIconUrl(const QString &iconUrl)
{
    if (iconUrl != _iconUrl) {
        _iconUrl = iconUrl;
    }
}

void Sharee::setType(const Type &type)
{
    if (type != _type) {
        _type = type;
    }
}

void Sharee::setIsIconColourful(const bool isColourful)
{
    if (_isIconColourful != isColourful) {
        _isIconColourful = isColourful;
        updateIconUrl();
    }
}

bool Sharee::updateIconUrl()
{
    if (_iconUrl.isEmpty() || !_isIconColourful) {
        return false;
    }

    const auto iconUrlColoured = _iconUrlColoured;
    _iconColor = (!_isIconColourful || !Theme::instance()->darkMode()) ? QStringLiteral("black") : QStringLiteral("white");
    _iconUrlColoured = QStringLiteral("image://svgimage-custom-color/") + _iconUrl + QStringLiteral("/") + _iconColor;

    return iconUrlColoured != _iconUrlColoured;
}

Sharee::Type Sharee::type() const
{
    return _type;
}

QString Sharee::iconUrl() const
{
    return _iconUrl;
}

QString Sharee::iconUrlColoured() const
{
    return _iconUrlColoured;
}

}
