/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWebSocket>
#include <QTimer>

#include "capabilities.h"

namespace OCC {

class Account;
class AbstractCredentials;

class OWNCLOUDSYNC_EXPORT PushNotifications : public QObject
{
    Q_OBJECT

public:
    explicit PushNotifications(Account *account, QObject *parent = nullptr);

    ~PushNotifications() override;

    /**
     * Setup push notifications
     *
     * This method needs to be called before push notifications can be used.
     */
    void setup();

    /**
     * Set the interval for reconnection attempts
     *
     * @param interval Interval in milliseconds.
     */
    void setReconnectTimerInterval(uint32_t interval);

    /**
     * Indicates if push notifications ready to use
     *
     * Ready to use means connected and authenticated.
     */
    [[nodiscard]] bool isReady() const;

    /**
     * Set the interval in which the websocket will ping the server if it is still alive.
     *
     * If the websocket does not respond in timeoutInterval, the connection will be terminated.
     *
     * @param interval Interval in milliseconds.
     */
    void setPingInterval(int interval);

signals:
    /**
     * Will be emitted after a successful connection and authentication
     */
    void ready();

    /**
     * Will be emitted if files on the server changed
     */
    void filesChanged(OCC::Account *account);

    /**
     * Will be emitted if specific files on the server changed
     */
    void fileIdsChanged(OCC::Account *account, const QList<qint64> &fileIds);

    /**
     * Will be emitted if activities have been changed on the server
     */
    void activitiesChanged(OCC::Account *account);

    /**
     * Will be emitted if notifications have been changed on the server
     */
    void notificationsChanged(OCC::Account *account);

    /**
     * Will be emitted if push notifications are unable to authenticate
     *
     * It's save to call #PushNotifications::setup() after this signal has been emitted.
     */
    void authenticationFailed();

    /**
     * Will be emitted if push notifications are unable to connect or the connection timed out
     *
     * It's save to call #PushNotifications::setup() after this signal has been emitted.
     */
    void connectionLost();

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketSslErrors(const QList<QSslError> &errors);
    void onWebSocketPongReceived(quint64 elapsedTime, const QByteArray &payload);
    void onPingTimedOut();

private:
    void openWebSocket();
    void reconnectToWebSocket();
    void closeWebSocket();
    void authenticateOnWebSocket();
    bool tryReconnectToWebSocket();
    void initReconnectTimer();
    void pingWebSocketServer();
    void startPingTimer();
    void startPingTimedOutTimer();

    void handleAuthenticated();
    void handleNotifyFile();
    void handleNotifyFileId(const QString &message);
    void handleInvalidCredentials();
    void handleNotifyNotification();
    void handleNotifyActivity();

    void emitFilesChanged();
    void emitNotificationsChanged();
    void emitActivitiesChanged();

    Account *_account = nullptr;
    QWebSocket *_webSocket;
    uint8_t _failedAuthenticationAttemptsCount = 0;
    QTimer *_reconnectTimer = nullptr;
    uint32_t _reconnectTimerInterval = 20 * 1000;
    bool _isReady = false;

    QTimer _pingTimer;
    QTimer _pingTimedOutTimer;
    bool _pongReceivedFromWebSocketServer = false;
};
}
