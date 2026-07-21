/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SERVERNOTIFICATIONHANDLER_H
#define SERVERNOTIFICATIONHANDLER_H

#include "tray/activitydata.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

class QJsonDocument;

namespace OCC {

class AccountState;
class JsonApiJob;

/** @brief Fetches and parses the Notifications app payload for one account. */
class ServerNotificationHandler : public QObject
{
    Q_OBJECT
public:
    /** @brief Create a one-shot server notification fetcher. */
    explicit ServerNotificationHandler(AccountState *accountState, QObject *parent = nullptr);

signals:
    /** @brief Emitted with the parsed Notifications app entries. */
    void newNotificationList(OCC::ActivityList);
    /** @brief Emitted with recent incoming-call entries. */
    void newIncomingCallsList(OCC::ActivityList);
    /** @brief Emitted when the one-shot fetch is complete. */
    void jobFinished();

public:
    /** @brief Start the fetch when the account supports notifications. */
    bool startFetchNotifications();

private slots:
    void slotNotificationsReceived(const QJsonDocument &json, int statusCode);
    void slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode);

private:
    QPointer<JsonApiJob> _notificationJob;
    AccountState *_accountState;
    QString _preFetchEtagHeader;
};
}

#endif // SERVERNOTIFICATIONHANDLER_H
