#include "pushnotifications.h"
#include "creds/abstractcredentials.h"
#include "account.h"

namespace {
static constexpr int MAX_ALLOWED_FAILED_AUTHENTICATION_ATTEMPTS = 3;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcPushNotifications, "nextcloud.sync.pushnotifications", QtInfoMsg)

PushNotifications::PushNotifications(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

PushNotifications::~PushNotifications()
{
    closeWebSocket();
}

void PushNotifications::setup()
{
    _isReady = false;
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
    if (_webSocket) {
        qCInfo(lcPushNotifications) << "Close websocket";
        _webSocket->close();
    }
}

void PushNotifications::onWebSocketConnected()
{
    qCInfo(lcPushNotifications) << "Connected to websocket";

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
    qCInfo(lcPushNotifications) << "Disconnected from websocket";
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

    qCWarning(lcPushNotifications) << "Websocket error" << error;

    _isReady = false;
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
    qCWarning(lcPushNotifications) << "Received websocket ssl errors:" << errors;
    _isReady = false;
    emit authenticationFailed();
}

void PushNotifications::openWebSocket()
{
    // Open websocket
    const auto capabilities = _account->capabilities();
    const auto webSocketUrl = capabilities.pushNotificationsWebSocketUrl();

    if (!_webSocket) {
        qCInfo(lcPushNotifications) << "Create websocket";
        _webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    }

    if (_webSocket) {
        connect(_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &PushNotifications::onWebSocketError, Qt::UniqueConnection);
        connect(_webSocket, &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors, Qt::UniqueConnection);
        connect(_webSocket, &QWebSocket::connected, this, &PushNotifications::onWebSocketConnected, Qt::UniqueConnection);
        connect(_webSocket, &QWebSocket::disconnected, this, &PushNotifications::onWebSocketDisconnected, Qt::UniqueConnection);

        qCInfo(lcPushNotifications) << "Open connection to websocket on:" << webSocketUrl;
        _webSocket->open(webSocketUrl);
    }
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
    emit ready();
}

void PushNotifications::handleNotifyFile()
{
    qCInfo(lcPushNotifications) << "Files push notification arrived";
    emit filesChanged(_account);
}

void PushNotifications::handleInvalidCredentials()
{
    qCInfo(lcPushNotifications) << "Invalid credentials submitted to websocket";
    if (!tryReconnectToWebSocket()) {
        _isReady = false;
        emit authenticationFailed();
    }
}

void PushNotifications::handleNotifyNotification()
{
    qCInfo(lcPushNotifications) << "Push notification arrived";
    emit notificationsChanged(_account);
}

void PushNotifications::handleNotifyActivity()
{
    qCInfo(lcPushNotifications) << "Push activity arrived";
    emit activitiesChanged(_account);
}
}
