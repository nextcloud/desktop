/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NOTIFICATIONHANDLER_H
#define NOTIFICATIONHANDLER_H

#include <QtCore>

#include "activitydata.h"

class QJsonDocument;

namespace OCC {

class AccountState;
class JsonApiJob;

class ServerNotificationHandler : public QObject
{
    Q_OBJECT
public:
    explicit ServerNotificationHandler(AccountState *accountState, QObject *parent = nullptr);

Q_SIGNALS:
    void newNotificationList(OCC::ActivityList);
    void newIncomingCallsList(OCC::ActivityList);
    void jobFinished();

public:
    bool startFetchNotifications();

private Q_SLOTS:
    void slotNotificationsReceived(const QJsonDocument &json, int statusCode);
    void slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode);

private:
    QPointer<JsonApiJob> _notificationJob;
    AccountState *_accountState;
    QString _preFetchEtagHeader;
};
}

#endif // NOTIFICATIONHANDLER_H
