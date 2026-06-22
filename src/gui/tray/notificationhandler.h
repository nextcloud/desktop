/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NOTIFICATIONHANDLER_H
#define NOTIFICATIONHANDLER_H

#include <QtCore>

#include "usermodel.h"

class QJsonDocument;

namespace OCC {

class ServerNotificationHandler : public QObject
{
    Q_OBJECT
public:
    explicit ServerNotificationHandler(AccountState *accountState, QObject *parent = nullptr);

signals:
    void newNotificationList(OCC::ActivityList);
    void newIncomingCallsList(OCC::ActivityList);
    void jobFinished();

public:
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

#endif // NOTIFICATIONHANDLER_H
