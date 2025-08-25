/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "userstatusconnector.h"
#include "theme.h"

namespace OCC {

UserStatus::UserStatus() = default;

UserStatus::UserStatus(
    const QString &id, const QString &message, const QString &icon,
    OnlineStatus state, bool messagePredefined, const Optional<ClearAt> &clearAt)
    : _id(id)
    , _message(message)
    , _icon(icon)
    , _state(state)
    , _messagePredefined(messagePredefined)
    , _clearAt(clearAt)
{
}

QString UserStatus::id() const
{
    return _id;
}

QString UserStatus::message() const
{
    return _message;
}

QString UserStatus::icon() const
{
    return _icon;
}

auto UserStatus::state() const -> OnlineStatus
{
    return _state;
}

bool UserStatus::messagePredefined() const
{
    return _messagePredefined;
}

QUrl UserStatus::stateIcon() const
{
    switch (_state) {
    case UserStatus::OnlineStatus::Away:
        return Theme::instance()->statusAwayImageSource();
    
    case UserStatus::OnlineStatus::Busy:
        return Theme::instance()->statusBusyImageSource();

    case UserStatus::OnlineStatus::DoNotDisturb:
        return Theme::instance()->statusDoNotDisturbImageSource();

    case UserStatus::OnlineStatus::Invisible:
    case UserStatus::OnlineStatus::Offline:
        return Theme::instance()->statusInvisibleImageSource();

    case UserStatus::OnlineStatus::Online:
        return Theme::instance()->statusOnlineImageSource();
    }

    Q_UNREACHABLE();
}

Optional<ClearAt> UserStatus::clearAt() const
{
    return _clearAt;
}

void UserStatus::setId(const QString &id)
{
    _id = id;
}

void UserStatus::setMessage(const QString &message)
{
    _message = message;
}

void UserStatus::setState(OnlineStatus state)
{
    _state = state;
}

void UserStatus::setIcon(const QString &icon)
{
    _icon = icon;
}

void UserStatus::setMessagePredefined(bool value)
{
    _messagePredefined = value;
}

void UserStatus::setClearAt(const Optional<ClearAt> &dateTime)
{
    _clearAt = dateTime;
}


UserStatusConnector::UserStatusConnector(QObject *parent)
    : QObject(parent)
{
}

UserStatusConnector::~UserStatusConnector() = default;
}
