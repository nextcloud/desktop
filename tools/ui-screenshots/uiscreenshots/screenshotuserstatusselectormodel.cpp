/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotuserstatusselectormodel.h"

#include "theme.h"

namespace OCC {

ScreenshotUserStatusSelectorModel::ScreenshotUserStatusSelectorModel(QObject *parent)
    : QObject(parent)
{
}

int ScreenshotUserStatusSelectorModel::userIndex() const
{
    return _userIndex;
}

void ScreenshotUserStatusSelectorModel::setUserIndex(const int userIndex)
{
    if (_userIndex == userIndex) {
        return;
    }
    _userIndex = userIndex;
    emit userIndexChanged();
}

QString ScreenshotUserStatusSelectorModel::userStatusMessage() const
{
    return _message;
}

void ScreenshotUserStatusSelectorModel::setUserStatusMessage(const QString &message)
{
    if (_message == message) {
        return;
    }
    _message = message;
    emit userStatusChanged();
}

QString ScreenshotUserStatusSelectorModel::userStatusEmoji() const
{
    return _emoji;
}

void ScreenshotUserStatusSelectorModel::setUserStatusEmoji(const QString &emoji)
{
    if (_emoji == emoji) {
        return;
    }
    _emoji = emoji;
    emit userStatusChanged();
}

int ScreenshotUserStatusSelectorModel::onlineStatus() const
{
    return _onlineStatus;
}

void ScreenshotUserStatusSelectorModel::setOnlineStatus(const int status)
{
    if (_onlineStatus == status) {
        return;
    }
    _onlineStatus = status;
    emit userStatusChanged();
    if (_finishOnOnlineStatusSet) {
        emit finished();
    }
}

QVariantList ScreenshotUserStatusSelectorModel::predefinedStatuses() const
{
    return {
        QVariantMap{
            {QStringLiteral("icon"), QStringLiteral("☕")},
            {QStringLiteral("message"), QStringLiteral("Taking a break")},
            {QStringLiteral("clearAt"), QStringLiteral("30 minutes")},
        },
        QVariantMap{
            {QStringLiteral("icon"), QStringLiteral("📅")},
            {QStringLiteral("message"), QStringLiteral("In a meeting")},
            {QStringLiteral("clearAt"), QStringLiteral("1 hour")},
        },
    };
}

QVariantList ScreenshotUserStatusSelectorModel::clearStageTypes() const
{
    return UserStatusSelectorModel{}.clearStageTypes();
}

QString ScreenshotUserStatusSelectorModel::clearAtDisplayString() const
{
    for (const auto &entry : clearStageTypes()) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("clearStageType")).toInt() == static_cast<int>(_clearStageType)) {
            return map.value(QStringLiteral("display")).toString();
        }
    }
    return {};
}

QString ScreenshotUserStatusSelectorModel::errorMessage() const
{
    return {};
}

bool ScreenshotUserStatusSelectorModel::busyStatusSupported() const
{
    return true;
}

bool ScreenshotUserStatusSelectorModel::userStatusLoaded() const
{
    return true;
}

bool ScreenshotUserStatusSelectorModel::finishOnOnlineStatusSet() const
{
    return _finishOnOnlineStatusSet;
}

void ScreenshotUserStatusSelectorModel::setFinishOnOnlineStatusSet(const bool finish)
{
    if (_finishOnOnlineStatusSet == finish) {
        return;
    }
    _finishOnOnlineStatusSet = finish;
    emit finishOnOnlineStatusSetChanged();
}

QUrl ScreenshotUserStatusSelectorModel::onlineIcon() const
{
    return Theme::instance()->statusOnlineImageSource();
}

QUrl ScreenshotUserStatusSelectorModel::awayIcon() const
{
    return Theme::instance()->statusAwayImageSource();
}

QUrl ScreenshotUserStatusSelectorModel::dndIcon() const
{
    return Theme::instance()->statusDoNotDisturbImageSource();
}

QUrl ScreenshotUserStatusSelectorModel::busyIcon() const
{
    return Theme::instance()->statusBusyImageSource();
}

QUrl ScreenshotUserStatusSelectorModel::invisibleIcon() const
{
    return Theme::instance()->statusInvisibleImageSource();
}

QString ScreenshotUserStatusSelectorModel::clearAtReadable(const QVariant &status) const
{
    return status.toMap().value(QStringLiteral("clearAt")).toString();
}

void ScreenshotUserStatusSelectorModel::setUserStatus()
{
    emit userStatusChanged();
    emit finished();
}

void ScreenshotUserStatusSelectorModel::clearUserStatus()
{
    _message.clear();
    _emoji.clear();
    emit userStatusChanged();
    emit finished();
}

void ScreenshotUserStatusSelectorModel::setClearAt(const QVariant &clearStageType)
{
    const auto selectedValue = clearStageType.toInt();
    for (const auto &entry : clearStageTypes()) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("clearStageType")).toInt() == selectedValue) {
            const auto selectedStage = static_cast<UserStatusSelectorModel::ClearStageType>(selectedValue);
            if (_clearStageType == selectedStage) {
                return;
            }
            _clearStageType = selectedStage;
            emit clearAtDisplayStringChanged();
            return;
        }
    }
}

void ScreenshotUserStatusSelectorModel::setPredefinedStatus(const QVariant &predefinedStatus)
{
    const auto map = predefinedStatus.toMap();
    _emoji = map.value(QStringLiteral("icon")).toString();
    _message = map.value(QStringLiteral("message")).toString();
    emit userStatusChanged();
}

}
