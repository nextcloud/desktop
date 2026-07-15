/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include "tray/activitydata.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSet>

class QNetworkReply;

namespace OCC {

class Account;
class ActivityListModel;
class Folder;

/**
 * @brief Owns notification fetching, delivery and action handling for one account.
 *
 * NotificationManager is the account-scoped boundary between server notifications,
 * activity-list presentation and platform notification centers.
 */
class NotificationManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Create the notification manager for an account and its activity model. */
    explicit NotificationManager(AccountStatePtr accountState, ActivityListModel *activityModel, QObject *parent = nullptr);
    /** @brief Unregister this account-scoped manager. */
    ~NotificationManager() override;

    /** @brief Return the live manager for a stable account id, or `nullptr`. */
    [[nodiscard]] static NotificationManager *forAccountId(const QString &accountId);

    /** @brief Show a desktop notification unless this id was shown recently. */
    void showNotification(const QString &title, const QString &message, qint64 notificationId);

    /** @brief Show an activity as a desktop notification. */
    void showNotification(const Activity &activity);

    /** @brief Add an account-owned activity notification without displaying it twice. */
    void addNotification(const Folder *folder, const Activity &activity);

    /** @brief Dismiss the notification at an activity-model row. */
    void dismissNotification(int activityIndex);

    /** @brief Trigger an action from the notification at an activity-model row. */
    void triggerNotificationAction(int activityIndex, int actionIndex);

    /** @brief Dismiss a server notification using its stable server identifier. */
    void dismissServerNotification(qint64 notificationId);

    /** @brief Trigger a server notification action using stable action metadata. */
    void triggerServerNotificationAction(qint64 notificationId, const QString &link, const QByteArray &verb);

    /** @brief Send a reply from an inline Talk notification action. */
    void sendTalkReply(const QString &reply, const QString &conversationToken, const QString &replyTo);

    /** @brief Send a Talk reply for an activity row and store the returned message. */
    void sendTalkReply(int activityIndex, const QString &conversationToken, const QString &message, const QString &replyTo);

    /** @brief Open the standalone Activities window for this manager's account. */
    void showActivities() const;

public slots:
    /** @brief Fetch the latest server notifications if no notification request is active. */
    void refresh();

    /** @brief Refresh when a push event belongs to this manager's account. */
    void handlePushNotification(OCC::Account *account);

    /** @brief Dispatch an activity-model notification action. */
    void sendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);

private slots:
    void buildNotificationDisplay(const OCC::ActivityList &list);
    void buildIncomingCallDialogs(const OCC::ActivityList &list);
    void notificationFetchFinished();
    void notificationRequestFinished(int statusCode);
    void notifyNetworkError(QNetworkReply *reply);
    void notifyServerFinished(const QString &reply, int replyCode);

private:
    void showNotification(const ActivityList &activityList);
    void showTalkNotification(const Activity &activity);
    void showServerNotification(const Activity &activity);
    void sendServerNotificationRequest(const QString &link, const QByteArray &verb, int row, qint64 notificationId);
    void endNotificationRequest(int replyCode);
    void checkNotifiedNotifications();
    [[nodiscard]] bool notificationAlreadyShown(qint64 notificationId);
    [[nodiscard]] bool canShowNotification(qint64 notificationId);

    AccountStatePtr _accountState;
    QPointer<ActivityListModel> _activityModel;
    QElapsedTimer _notificationHistoryTimer;
    QSet<qint64> _notifiedNotifications;
    int _notificationRequestsRunning = 0;
    bool _isNotificationFetchRunning = false;
};

}
