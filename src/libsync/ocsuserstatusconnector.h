/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include "userstatusconnector.h"

#include <QPointer>

namespace OCC {

class JsonApiJob;
class SimpleNetworkJob;

class OWNCLOUDSYNC_EXPORT OcsUserStatusConnector : public UserStatusConnector
{
public:
    explicit OcsUserStatusConnector(AccountPtr account, QObject *parent = nullptr);

    void fetchUserStatus() override;

    void fetchPredefinedStatuses() override;

    void setUserStatus(const UserStatus &userStatus) override;

    void clearMessage() override;

    [[nodiscard]] UserStatus userStatus() const override;

    [[nodiscard]] bool supportsBusyStatus() const override;

private:
    void onUserStatusFetched(const QJsonDocument &json, int statusCode);
    void onPredefinedStatusesFetched(const QJsonDocument &json, int statusCode);
    void onUserStatusOnlineStatusSet(const QJsonDocument &json, int statusCode);
    void onUserStatusMessageSet(const QJsonDocument &json, int statusCode);
    void onMessageCleared(const QJsonDocument &json, int statusCode);

    void logResponse(const QString &message, const QJsonDocument &json, int statusCode);
    void startFetchUserStatusJob();
    void startFetchPredefinedStatuses();
    void setUserStatusOnlineStatus(UserStatus::OnlineStatus onlineStatus);
    void setUserStatusMessage(const UserStatus &userStatus);
    void setUserStatusMessagePredefined(const UserStatus &userStatus);
    void setUserStatusMessageCustom(const UserStatus &userStatus);

    AccountPtr _account;

    bool _userStatusSupported = false;
    bool _userStatusEmojisSupported = false;
    bool _userStatusBusySupported = false;

    QPointer<JsonApiJob> _clearMessageJob {};
    QPointer<JsonApiJob> _setMessageJob {};
    QPointer<JsonApiJob> _setOnlineStatusJob {};
    QPointer<JsonApiJob> _getPredefinedStausesJob {};
    QPointer<JsonApiJob> _getUserStatusJob {};

    UserStatus _userStatus;
};
}
