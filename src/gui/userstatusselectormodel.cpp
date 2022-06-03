/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "userstatusselectormodel.h"
#include "tray/usermodel.h"

#include <qnamespace.h>
#include <theme.h>

#include <QDateTime>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace OCC {

Q_LOGGING_CATEGORY(lcUserStatusDialogModel, "nextcloud.gui.userstatusdialogmodel", QtInfoMsg)

UserStatusSelectorModel::UserStatusSelectorModel(QObject *parent)
    : QObject(parent)
    , _dateTimeProvider(new DateTimeProvider)
{
    _userStatus.setIcon("😀");
}

UserStatusSelectorModel::UserStatusSelectorModel(std::shared_ptr<UserStatusConnector> userStatusConnector, QObject *parent)
    : QObject(parent)
    , _userStatusConnector(userStatusConnector)
    , _userStatus("no-id", "", "😀", UserStatus::OnlineStatus::Online, false, {})
    , _dateTimeProvider(new DateTimeProvider)
{
    _userStatus.setIcon("😀");
    init();
}

UserStatusSelectorModel::UserStatusSelectorModel(std::shared_ptr<UserStatusConnector> userStatusConnector,
    std::unique_ptr<DateTimeProvider> dateTimeProvider,
    QObject *parent)
    : QObject(parent)
    , _userStatusConnector(userStatusConnector)
    , _dateTimeProvider(std::move(dateTimeProvider))
{
    _userStatus.setIcon("😀");
    init();
}

UserStatusSelectorModel::UserStatusSelectorModel(const UserStatus &userStatus,
    std::unique_ptr<DateTimeProvider> dateTimeProvider, QObject *parent)
    : QObject(parent)
    , _userStatus(userStatus)
    , _dateTimeProvider(std::move(dateTimeProvider))
{
    _userStatus.setIcon("😀");
}

UserStatusSelectorModel::UserStatusSelectorModel(const UserStatus &userStatus,
    QObject *parent)
    : QObject(parent)
    , _userStatus(userStatus)
{
    _userStatus.setIcon("😀");
}

int UserStatusSelectorModel::userIndex() const
{
    return _userIndex;
}

void UserStatusSelectorModel::setUserIndex(const int userIndex)
{
    if(userIndex < 0) {
        qCWarning(lcUserStatusDialogModel) << "Invalid user index: " << _userIndex;
        return;
    }

    reset();

    _userIndex = userIndex;
    emit userIndexChanged();

    qCDebug(lcUserStatusDialogModel) << "Loading user status connector for user with index: " << _userIndex;
    _userStatusConnector = UserModel::instance()->userStatusConnector(_userIndex);

    init();
}

void UserStatusSelectorModel::reset()
{
    if (_userStatusConnector) {
        disconnect(_userStatusConnector.get(), &UserStatusConnector::userStatusFetched, this,
            &UserStatusSelectorModel::onUserStatusFetched);
        disconnect(_userStatusConnector.get(), &UserStatusConnector::predefinedStatusesFetched, this,
            &UserStatusSelectorModel::onPredefinedStatusesFetched);
        disconnect(_userStatusConnector.get(), &UserStatusConnector::error, this,
            &UserStatusSelectorModel::onError);
        disconnect(_userStatusConnector.get(), &UserStatusConnector::userStatusSet, this,
            &UserStatusSelectorModel::onUserStatusSet);
        disconnect(_userStatusConnector.get(), &UserStatusConnector::messageCleared, this,
            &UserStatusSelectorModel::onMessageCleared);
    }
    _userStatusConnector = nullptr;
}

void UserStatusSelectorModel::init()
{
    if (!_userStatusConnector) {
        qCWarning(lcUserStatusDialogModel) << "No user status connector set";
        return;
    }

    connect(_userStatusConnector.get(), &UserStatusConnector::userStatusFetched, this,
        &UserStatusSelectorModel::onUserStatusFetched);
    connect(_userStatusConnector.get(), &UserStatusConnector::predefinedStatusesFetched, this,
        &UserStatusSelectorModel::onPredefinedStatusesFetched);
    connect(_userStatusConnector.get(), &UserStatusConnector::error, this,
        &UserStatusSelectorModel::onError);
    connect(_userStatusConnector.get(), &UserStatusConnector::userStatusSet, this,
        &UserStatusSelectorModel::onUserStatusSet);
    connect(_userStatusConnector.get(), &UserStatusConnector::messageCleared, this,
        &UserStatusSelectorModel::onMessageCleared);

    _userStatusConnector->fetchUserStatus();
    _userStatusConnector->fetchPredefinedStatuses();
}

void UserStatusSelectorModel::onUserStatusSet()
{
    emit finished();
}

void UserStatusSelectorModel::onMessageCleared()
{
    emit finished();
}

void UserStatusSelectorModel::onError(UserStatusConnector::Error error)
{
    qCWarning(lcUserStatusDialogModel) << "Error:" << error;

    switch (error) {
    case UserStatusConnector::Error::CouldNotFetchPredefinedUserStatuses:
        setError(tr("Could not fetch predefined statuses. Make sure you are connected to the server."));
        return;

    case UserStatusConnector::Error::CouldNotFetchUserStatus:
        setError(tr("Could not fetch status. Make sure you are connected to the server."));
        return;

    case UserStatusConnector::Error::UserStatusNotSupported:
        setError(tr("Status feature is not supported. You will not be able to set your status."));
        return;

    case UserStatusConnector::Error::EmojisNotSupported:
        setError(tr("Emojis are not supported. Some status functionality may not work."));
        return;

    case UserStatusConnector::Error::CouldNotSetUserStatus:
        setError(tr("Could not set status. Make sure you are connected to the server."));
        return;

    case UserStatusConnector::Error::CouldNotClearMessage:
        setError(tr("Could not clear status message. Make sure you are connected to the server."));
        return;
    }

    Q_UNREACHABLE();
}

void UserStatusSelectorModel::setError(const QString &reason)
{
    _errorMessage = reason;
    emit errorMessageChanged();
}

void UserStatusSelectorModel::clearError()
{
    setError("");
}

void UserStatusSelectorModel::setOnlineStatus(UserStatus::OnlineStatus status)
{
    if (!_userStatusConnector || status == _userStatus.state()) {
        return;
    }

    _userStatus.setState(status);
    _userStatusConnector->setUserStatus(_userStatus);
    emit userStatusChanged();
}

QUrl UserStatusSelectorModel::onlineIcon() const
{
    return Theme::instance()->statusOnlineImageSource();
}

QUrl UserStatusSelectorModel::awayIcon() const
{
    return Theme::instance()->statusAwayImageSource();
}
QUrl UserStatusSelectorModel::dndIcon() const
{
    return Theme::instance()->statusDoNotDisturbImageSource();
}
QUrl UserStatusSelectorModel::invisibleIcon() const
{
    return Theme::instance()->statusInvisibleImageSource();
}

UserStatus::OnlineStatus UserStatusSelectorModel::onlineStatus() const
{
    return _userStatus.state();
}

QString UserStatusSelectorModel::userStatusMessage() const
{
    return _userStatus.message();
}

void UserStatusSelectorModel::setUserStatusMessage(const QString &message)
{
    _userStatus.setMessage(message);
    _userStatus.setMessagePredefined(false);
    emit userStatusChanged();
}

void UserStatusSelectorModel::setUserStatusEmoji(const QString &emoji)
{
    _userStatus.setIcon(emoji);
    _userStatus.setMessagePredefined(false);
    emit userStatusChanged();
}

QString UserStatusSelectorModel::userStatusEmoji() const
{
    return _userStatus.icon();
}

void UserStatusSelectorModel::onUserStatusFetched(const UserStatus &userStatus)
{
    _userStatus.setState(userStatus.state());
    _userStatus.setMessage(userStatus.message());
    _userStatus.setMessagePredefined(userStatus.messagePredefined());
    _userStatus.setId(userStatus.id());
    _userStatus.setClearAt(userStatus.clearAt());

    if (!userStatus.icon().isEmpty()) {
        _userStatus.setIcon(userStatus.icon());
    }

    emit userStatusChanged();
    emit clearAtDisplayStringChanged();
}

Optional<ClearAt> UserStatusSelectorModel::clearStageTypeToDateTime(ClearStageType type) const
{
    switch (type) {
    case ClearStageType::DontClear:
        return {};

    case ClearStageType::HalfHour: {
        ClearAt clearAt;
        clearAt._type = ClearAtType::Period;
        clearAt._period = 60 * 30;
        return clearAt;
    }

    case ClearStageType::OneHour: {
        ClearAt clearAt;
        clearAt._type = ClearAtType::Period;
        clearAt._period = 60 * 60;
        return clearAt;
    }

    case ClearStageType::FourHour: {
        ClearAt clearAt;
        clearAt._type = ClearAtType::Period;
        clearAt._period = 60 * 60 * 4;
        return clearAt;
    }

    case ClearStageType::Today: {
        ClearAt clearAt;
        clearAt._type = ClearAtType::EndOf;
        clearAt._endof = "day";
        return clearAt;
    }

    case ClearStageType::Week: {
        ClearAt clearAt;
        clearAt._type = ClearAtType::EndOf;
        clearAt._endof = "week";
        return clearAt;
    }

    default:
        Q_UNREACHABLE();
    }
}

void UserStatusSelectorModel::setUserStatus()
{
    if (!_userStatusConnector) {
        return;
    }

    clearError();
    _userStatusConnector->setUserStatus(_userStatus);
}

void UserStatusSelectorModel::clearUserStatus()
{
    if (!_userStatusConnector) {
        return;
    }

    clearError();
    _userStatusConnector->clearMessage();
}

void UserStatusSelectorModel::onPredefinedStatusesFetched(const QVector<UserStatus> &statuses)
{
    _predefinedStatuses = statuses;
    emit predefinedStatusesChanged();
}

QVector<UserStatus> UserStatusSelectorModel::predefinedStatuses() const
{
    return _predefinedStatuses;
}

void UserStatusSelectorModel::setPredefinedStatus(const UserStatus &predefinedStatus)
{
    _userStatus.setMessagePredefined(true);
    _userStatus.setId(predefinedStatus.id());
    _userStatus.setMessage(predefinedStatus.message());
    _userStatus.setIcon(predefinedStatus.icon());
    _userStatus.setClearAt(predefinedStatus.clearAt());

    emit userStatusChanged();
    emit clearAtDisplayStringChanged();
}

QString UserStatusSelectorModel::clearAtStageToString(ClearStageType stage) const
{
    switch (stage) {
    case ClearStageType::DontClear:
        return tr("Don't clear");

    case ClearStageType::HalfHour:
        return tr("30 minutes");

    case ClearStageType::OneHour:
        return tr("1 hour");

    case ClearStageType::FourHour:
        return tr("4 hours");

    case ClearStageType::Today:
        return tr("Today");

    case ClearStageType::Week:
        return tr("This week");

    default:
        Q_UNREACHABLE();
    }
}

QVariantList UserStatusSelectorModel::clearStageTypes() const
{
    QVariantList clearStageTypes;

    for(const auto clearStageType : _clearStages) {
        QVariantMap clearStageToAdd;
        clearStageToAdd.insert(QStringLiteral("display"), clearAtStageToString(clearStageType));
        clearStageToAdd.insert(QStringLiteral("clearStageType"), QVariant::fromValue(clearStageType));
        clearStageTypes.append(clearStageToAdd);
    }

    return clearStageTypes;
}

void UserStatusSelectorModel::setClearAt(const ClearStageType clearStageType)
{
    _userStatus.setClearAt(clearStageTypeToDateTime(clearStageType));
    emit clearAtDisplayStringChanged();
}

QString UserStatusSelectorModel::errorMessage() const
{
    return _errorMessage;
}

QString UserStatusSelectorModel::timeDifferenceToString(int differenceSecs) const
{
    if (differenceSecs < 60) {
        return tr("Less than a minute");
    } else if (differenceSecs < 60 * 60) {
        const auto minutesLeft = static_cast<int>(std::ceil(differenceSecs / 60.0));
        if (minutesLeft == 1) {
            return tr("1 minute");
        } else {
            return tr("%1 minutes", "", minutesLeft).arg(minutesLeft);
        }
    } else if (differenceSecs < 60 * 60 * 24) {
        const auto hoursLeft = static_cast<int>(std::ceil(differenceSecs / 60.0 / 60.0));
        if (hoursLeft == 1) {
            return tr("1 hour");
        } else {
            return tr("%1 hours", "", hoursLeft).arg(hoursLeft);
        }
    } else {
        const auto daysLeft = static_cast<int>(std::ceil(differenceSecs / 60.0 / 60.0 / 24.0));
        if (daysLeft == 1) {
            return tr("1 day");
        } else {
            return tr("%1 days", "", daysLeft).arg(daysLeft);
        }
    }
}

QString UserStatusSelectorModel::clearAtReadable(const UserStatus &status) const
{
    const auto clearAt = status.clearAt();
    return clearAtReadable(clearAt);
}

QString UserStatusSelectorModel::clearAtReadable(const Optional<ClearAt> &clearAt) const
{
    if (clearAt) {
        switch (clearAt->_type) {
        case ClearAtType::Period: {
            return timeDifferenceToString(clearAt->_period);
        }

        case ClearAtType::Timestamp: {
            const int difference = static_cast<int>(clearAt->_timestamp - _dateTimeProvider->currentDateTime().toSecsSinceEpoch());
            return timeDifferenceToString(difference);
        }

        case ClearAtType::EndOf: {
            if (clearAt->_endof == "day") {
                return tr("Today");
            } else if (clearAt->_endof == "week") {
                return tr("This week");
            }
            Q_UNREACHABLE();
        }

        default:
            Q_UNREACHABLE();
        }
    }
    return tr("Don't clear");
}

QString UserStatusSelectorModel::clearAtDisplayString() const
{
    return clearAtReadable(_userStatus.clearAt());
}

}
