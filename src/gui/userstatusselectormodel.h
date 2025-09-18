/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    Q_PROPERTY(bool busyStatusSupported READ busyStatusSupported NOTIFY busyStatusSupportedChanged)
    Q_PROPERTY(QUrl onlineIcon READ onlineIcon CONSTANT)
    Q_PROPERTY(QUrl awayIcon READ awayIcon CONSTANT)
    Q_PROPERTY(QUrl dndIcon READ dndIcon CONSTANT)
    Q_PROPERTY(QUrl busyIcon READ busyIcon CONSTANT)
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

    [[nodiscard]] int userIndex() const;

    [[nodiscard]] UserStatus::OnlineStatus onlineStatus() const;
    void setOnlineStatus(UserStatus::OnlineStatus status);

    [[nodiscard]] QUrl onlineIcon() const;
    [[nodiscard]] QUrl awayIcon() const;
    [[nodiscard]] QUrl dndIcon() const;
    [[nodiscard]] QUrl busyIcon() const;
    [[nodiscard]] QUrl invisibleIcon() const;

    [[nodiscard]] QString userStatusMessage() const;
    void setUserStatusMessage(const QString &message);
    [[nodiscard]] QString userStatusEmoji() const;
    void setUserStatusEmoji(const QString &emoji);

    [[nodiscard]] QVector<UserStatus> predefinedStatuses() const;

    [[nodiscard]] QVariantList clearStageTypes() const;
    [[nodiscard]] QString clearAtDisplayString() const;
    [[nodiscard]] Q_INVOKABLE QString clearAtReadable(const OCC::UserStatus &status) const;

    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] bool busyStatusSupported() const;

public slots:
    void setUserIndex(const int userIndex);
    void setUserStatus();
    void clearUserStatus();
    void setClearAt(const OCC::UserStatusSelectorModel::ClearStageType clearStageType);
    void setPredefinedStatus(const OCC::UserStatus &predefinedStatus);

signals:
    void userIndexChanged();
    void errorMessageChanged();
    void busyStatusSupportedChanged();
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

    [[nodiscard]] QString clearAtReadable(const Optional<ClearAt> &clearAt) const;
    [[nodiscard]] QString clearAtStageToString(ClearStageType stage) const;
    [[nodiscard]] QString timeDifferenceToString(int differenceSecs) const;
    [[nodiscard]] Optional<ClearAt> clearStageTypeToDateTime(ClearStageType type) const;
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
