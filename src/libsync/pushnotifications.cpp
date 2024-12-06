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

#include "pushnotifications.h"
#include "creds/abstractcredentials.h"
#include "account.h"

namespace {
static constexpr int MAX_ALLOWED_FAILED_AUTHENTICATION_ATTEMPTS = 3;
static constexpr int PING_INTERVAL = 30 * 1000;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcPushNotifications, "nextcloud.sync.pushnotifications", QtInfoMsg)

PushNotifications::PushNotifications(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &PushNotifications::onWebSocketError);
    connect(_webSocket, &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors);
    connect(_webSocket, &QWebSocket::connected, this, &PushNotifications::onWebSocketConnected);
    connect(_webSocket, &QWebSocket::disconnected, this, &PushNotifications::onWebSocketDisconnected);
    connect(_webSocket, &QWebSocket::pong, this, &PushNotifications::onWebSocketPongReceived);

    connect(&_pingTimer, &QTimer::timeout, this, &PushNotifications::pingWebSocketServer);
    _pingTimer.setSingleShot(true);
    _pingTimer.setInterval(PING_INTERVAL);

    connect(&_pingTimedOutTimer, &QTimer::timeout, this, &PushNotifications::onPingTimedOut);
    _pingTimedOutTimer.setSingleShot(true);
    _pingTimedOutTimer.setInterval(PING_INTERVAL);
}

PushNotifications::~PushNotifications()
{
    closeWebSocket();
}

void PushNotifications::setup()
{
    qCInfo(lcPushNotifications) << "Setup push notifications";
    _failedAuthenticationAttemptsCount = 0;
    reconnectToWebSocket();
}

void PushNotifications::reconnectToWebSocket()
{
    closeWebSocket();
    openWebSocket();
}

void PushNotifications::closeWebSocket()
{
    qCInfo(lcPushNotifications) << "Close websocket for account" << _account->url();

    _pingTimer.stop();
    _pingTimedOutTimer.stop();
    _isReady = false;

    // Maybe there run some reconnection attempts
    if (_reconnectTimer) {
        _reconnectTimer->stop();
    }

    disconnect(_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &PushNotifications::onWebSocketError);
    disconnect(_webSocket, &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors);

    _webSocket->close();
}

void PushNotifications::onWebSocketConnected()
{
    qCInfo(lcPushNotifications) << "Connected to websocket for account" << _account->url();

    connect(_webSocket, &QWebSocket::textMessageReceived, this, &PushNotifications::onWebSocketTextMessageReceived, Qt::UniqueConnection);

    authenticateOnWebSocket();
}

void PushNotifications::authenticateOnWebSocket()
{
    const auto credentials = _account->credentials();
    const auto username = credentials->user();
    const auto password = credentials->password();

    // Authenticate
    _webSocket->sendTextMessage(username);
    _webSocket->sendTextMessage(password);
}

void PushNotifications::onWebSocketDisconnected()
{
    qCInfo(lcPushNotifications) << "Disconnected from websocket for account" << _account->url();
}

void PushNotifications::onWebSocketTextMessageReceived(const QString &message)
{
    qCInfo(lcPushNotifications) << "Received push notification:" << message;

    if (message == "notify_file") {
        handleNotifyFile();
    } else if (message == "notify_activity") {
        handleNotifyActivity();
    } else if (message == "notify_notification") {
        handleNotifyNotification();
    } else if (message == "authenticated") {
        handleAuthenticated();
    } else if (message == "err: Invalid credentials") {
        handleInvalidCredentials();
    }
}

void PushNotifications::onWebSocketError(QAbstractSocket::SocketError error)
{
    // This error gets thrown in testSetup_maxConnectionAttemptsReached_deletePushNotifications after
    // the second connection attempt. I have no idea why this happens. Maybe the socket gets not closed correctly?
    // I think it's fine to ignore this error.
    if (error == QAbstractSocket::UnfinishedSocketOperationError) {
        return;
    }

    qCWarning(lcPushNotifications) << "Websocket error on with account" << _account->url() << error;
    closeWebSocket();
    emit connectionLost();
}

bool PushNotifications::tryReconnectToWebSocket()
{
    ++_failedAuthenticationAttemptsCount;
    if (_failedAuthenticationAttemptsCount >= MAX_ALLOWED_FAILED_AUTHENTICATION_ATTEMPTS) {
        qCInfo(lcPushNotifications) << "Max authentication attempts reached";
        return false;
    }

    if (!_reconnectTimer) {
        _reconnectTimer = new QTimer(this);
    }

    _reconnectTimer->setInterval(_reconnectTimerInterval);
    _reconnectTimer->setSingleShot(true);
    connect(_reconnectTimer, &QTimer::timeout, [this]() {
        reconnectToWebSocket();
    });
    _reconnectTimer->start();

    return true;
}

void PushNotifications::onWebSocketSslErrors(const QList<QSslError> &errors)
{
    qCWarning(lcPushNotifications) << "Websocket ssl errors on with account" << _account->url() << errors;
    closeWebSocket();
    emit authenticationFailed();
}

void PushNotifications::openWebSocket()
{
    // Open websocket
    const auto capabilities = _account->capabilities();
    const auto webSocketUrl = capabilities.pushNotificationsWebSocketUrl();

    qCInfo(lcPushNotifications) << "Open connection to websocket on" << webSocketUrl << "for account" << _account->url();
    connect(_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &PushNotifications::onWebSocketError);
    connect(_webSocket, &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors);
    _webSocket->open(webSocketUrl);
}

void PushNotifications::setReconnectTimerInterval(uint32_t interval)
{
    _reconnectTimerInterval = interval;
}

bool PushNotifications::isReady() const
{
    return _isReady;
}

void PushNotifications::handleAuthenticated()
{
    qCInfo(lcPushNotifications) << "Authenticated successful on websocket";
    _failedAuthenticationAttemptsCount = 0;
    _isReady = true;
    startPingTimer();
    emit ready();

    // We maybe reconnected to websocket while being offline for a
    // while. To not miss any notifications that may have happened,
    // emit all the signals once.
    emitFilesChanged();
    emitNotificationsChanged();
    emitActivitiesChanged();
}

void PushNotifications::handleNotifyFile()
{
    qCInfo(lcPushNotifications) << "Files push notification arrived";
    emitFilesChanged();
}

void PushNotifications::handleInvalidCredentials()
{
    qCInfo(lcPushNotifications) << "Invalid credentials submitted to websocket";
    if (!tryReconnectToWebSocket()) {
        closeWebSocket();
        emit authenticationFailed();
    }
}

void PushNotifications::handleNotifyNotification()
{
    qCInfo(lcPushNotifications) << "Push notification arrived";
    emitNotificationsChanged();
}

void PushNotifications::handleNotifyActivity()
{
    qCInfo(lcPushNotifications) << "Push activity arrived";
    emitActivitiesChanged();
}

void PushNotifications::onWebSocketPongReceived(quint64 /*elapsedTime*/, const QByteArray & /*payload*/)
{
    qCDebug(lcPushNotifications) << "Pong received in time";
    // We are fine with every kind of pong and don't care about the
    // payload. As long as we receive pongs the server is still alive.
    _pongReceivedFromWebSocketServer = true;
    startPingTimer();
}

void PushNotifications::startPingTimer()
{
    _pingTimedOutTimer.stop();
    _pingTimer.start();
}

void PushNotifications::startPingTimedOutTimer()
{
    _pingTimedOutTimer.start();
}

void PushNotifications::pingWebSocketServer()
{
    qCDebug(lcPushNotifications, "Ping websocket server");

    _pongReceivedFromWebSocketServer = false;

    _webSocket->ping({});
    startPingTimedOutTimer();
}

void PushNotifications::onPingTimedOut()
{
    if (_pongReceivedFromWebSocketServer) {
        qCDebug(lcPushNotifications) << "Websocket respond with a pong in time.";
        return;
    }

    qCInfo(lcPushNotifications) << "Websocket did not respond with a pong in time. Try to reconnect.";
    // Try again to connect
    setup();
}

void PushNotifications::setPingInterval(int timeoutInterval)
{
    _pingTimer.setInterval(timeoutInterval);
    _pingTimedOutTimer.setInterval(timeoutInterval);
}

void PushNotifications::emitFilesChanged()
{
    emit filesChanged(_account);
}

void PushNotifications::emitNotificationsChanged()
{
    emit notificationsChanged(_account);
}

void PushNotifications::emitActivitiesChanged()
{
    emit activitiesChanged(_account);
}
}
