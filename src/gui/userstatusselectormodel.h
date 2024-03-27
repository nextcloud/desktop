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

#pragma once

#include "common/result.h"

#include <userstatusconnector.h>
#include <ocsuserstatusconnector.h>
#include <datetimeprovider.h>

#include <QObject>
#include <QMetaType>
#include <QtNumeric>

#include <cstddef>
#include <memory>
#include <vector>

namespace OCC {

class UserStatusSelectorModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int userIndex READ userIndex WRITE setUserIndex NOTIFY userIndexChanged)
    Q_PROPERTY(QString userStatusMessage READ userStatusMessage WRITE setUserStatusMessage NOTIFY userStatusChanged)
    Q_PROPERTY(QString userStatusEmoji READ userStatusEmoji WRITE setUserStatusEmoji NOTIFY userStatusChanged)
    Q_PROPERTY(OCC::UserStatus::OnlineStatus onlineStatus READ onlineStatus WRITE setOnlineStatus NOTIFY userStatusChanged)
    Q_PROPERTY(QVector<OCC::UserStatus> predefinedStatuses READ predefinedStatuses NOTIFY predefinedStatusesChanged)
    Q_PROPERTY(QVariantList clearStageTypes READ clearStageTypes CONSTANT)
    Q_PROPERTY(QString clearAtDisplayString READ clearAtDisplayString NOTIFY clearAtDisplayStringChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QUrl onlineIcon READ onlineIcon CONSTANT)
    Q_PROPERTY(QUrl awayIcon READ awayIcon CONSTANT)
    Q_PROPERTY(QUrl dndIcon READ dndIcon CONSTANT)
    Q_PROPERTY(QUrl invisibleIcon READ invisibleIcon CONSTANT)

public:
    enum class ClearStageType {
        DontClear,
        HalfHour,
        OneHour,
        FourHour,
        Today,
        Week,
    };
    Q_ENUM(ClearStageType);

    explicit UserStatusSelectorModel(QObject *parent = nullptr);

    explicit UserStatusSelectorModel(std::shared_ptr<UserStatusConnector> userStatusConnector,
        QObject *parent = nullptr);

    explicit UserStatusSelectorModel(std::shared_ptr<UserStatusConnector> userStatusConnector,
        std::unique_ptr<DateTimeProvider> dateTimeProvider,
        QObject *parent = nullptr);

    explicit UserStatusSelectorModel(const UserStatus &userStatus,
        std::unique_ptr<DateTimeProvider> dateTimeProvider,
        QObject *parent = nullptr);

    explicit UserStatusSelectorModel(const UserStatus &userStatus,
        QObject *parent = nullptr);

    Q_REQUIRED_RESULT int userIndex() const;

    Q_REQUIRED_RESULT UserStatus::OnlineStatus onlineStatus() const;
    void setOnlineStatus(UserStatus::OnlineStatus status);

    Q_REQUIRED_RESULT QUrl onlineIcon() const;
    Q_REQUIRED_RESULT QUrl awayIcon() const;
    Q_REQUIRED_RESULT QUrl dndIcon() const;
    Q_REQUIRED_RESULT QUrl invisibleIcon() const;

    Q_REQUIRED_RESULT QString userStatusMessage() const;
    void setUserStatusMessage(const QString &message);
    Q_REQUIRED_RESULT QString userStatusEmoji() const;
    void setUserStatusEmoji(const QString &emoji);

    [[nodiscard]] QVector<UserStatus> predefinedStatuses() const;

    Q_REQUIRED_RESULT QVariantList clearStageTypes() const;
    Q_REQUIRED_RESULT QString clearAtDisplayString() const;
    [[nodiscard]] Q_INVOKABLE QString clearAtReadable(const OCC::UserStatus &status) const;

    Q_REQUIRED_RESULT QString errorMessage() const;

public slots:
    void setUserIndex(const int userIndex);
    void setUserStatus();
    void clearUserStatus();
    void setClearAt(const OCC::UserStatusSelectorModel::ClearStageType clearStageType);
    void setPredefinedStatus(const OCC::UserStatus &predefinedStatus);

signals:
    void userIndexChanged();
    void errorMessageChanged();
    void userStatusChanged();
    void clearAtDisplayStringChanged();
    void predefinedStatusesChanged();
    void finished();

private:
    void init();
    void reset();
    void onUserStatusFetched(const UserStatus &userStatus);
    void onPredefinedStatusesFetched(const QVector<UserStatus> &statuses);
    void onUserStatusSet();
    void onMessageCleared();
    void onError(UserStatusConnector::Error error);

    Q_REQUIRED_RESULT QString clearAtReadable(const Optional<ClearAt> &clearAt) const;
    Q_REQUIRED_RESULT QString clearAtStageToString(ClearStageType stage) const;
    Q_REQUIRED_RESULT QString timeDifferenceToString(int differenceSecs) const;
    Q_REQUIRED_RESULT Optional<ClearAt> clearStageTypeToDateTime(ClearStageType type) const;
    void setError(const QString &reason);
    void clearError();

    int _userIndex = -1;
    std::shared_ptr<UserStatusConnector> _userStatusConnector {};
    QVector<UserStatus> _predefinedStatuses;
    UserStatus _userStatus;
    std::unique_ptr<DateTimeProvider> _dateTimeProvider;

    QString _errorMessage;

    std::vector<ClearStageType> _clearStages = {
        ClearStageType::DontClear,
        ClearStageType::HalfHour,
        ClearStageType::OneHour,
        ClearStageType::FourHour,
        ClearStageType::Today,
        ClearStageType::Week
    };
};
}
