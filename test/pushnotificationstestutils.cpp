#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>
#include <cstdint>
#include <functional>

#include "pushnotificationstestutils.h"
#include "pushnotifications.h"

Q_LOGGING_CATEGORY(lcFakeWebSocketServer, "nextcloud.test.fakewebserver", QtInfoMsg)

FakeWebSocketServer::FakeWebSocketServer(quint16 port, QObject *parent)
    : QObject(parent)
    , _webSocketServer(new QWebSocketServer(QStringLiteral("Fake Server"), QWebSocketServer::NonSecureMode, this))
{
    if (!_webSocketServer->listen(QHostAddress::LocalHost, port)) {
        Q_UNREACHABLE();
    }
    connect(_webSocketServer, &QWebSocketServer::newConnection, this, &FakeWebSocketServer::onNewConnection);
    connect(_webSocketServer, &QWebSocketServer::closed, this, &FakeWebSocketServer::closed);
    qCInfo(lcFakeWebSocketServer) << "Open fake websocket server on port:" << port;
    _processTextMessageSpy = std::make_unique<QSignalSpy>(this, &FakeWebSocketServer::processTextMessage);
}

FakeWebSocketServer::~FakeWebSocketServer()
{
    close();
}

QWebSocket *FakeWebSocketServer::authenticateAccount(const OCC::AccountPtr account, std::function<void(OCC::PushNotifications *pushNotifications)> beforeAuthentication, std::function<void(void)> afterAuthentication)
{
    const auto pushNotifications = account->pushNotifications();
    Q_ASSERT(pushNotifications);
    QSignalSpy readySpy(pushNotifications, &OCC::PushNotifications::ready);

    beforeAuthentication(pushNotifications);

    // Wait for authentication
    if (!waitForTextMessages()) {
        return nullptr;
    }

    // Right authentication data should be sent
    if (textMessagesCount() != 2) {
        return nullptr;
    }

    const auto socket = socketForTextMessage(0);
    const auto userSent = textMessage(0);
    const auto passwordSent = textMessage(1);

    if (userSent != account->credentials()->user() || passwordSent != account->credentials()->password()) {
        return nullptr;
    }

    // Sent authenticated
    socket->sendTextMessage("authenticated");

    // Wait for ready signal
    readySpy.wait();
    if (readySpy.count() != 1 || !account->pushNotifications()->isReady()) {
        return nullptr;
    }

    afterAuthentication();

    return socket;
}

void FakeWebSocketServer::close()
{
    if (_webSocketServer->isListening()) {
        qCInfo(lcFakeWebSocketServer) << "Close fake websocket server";

        _webSocketServer->close();
        qDeleteAll(_clients.begin(), _clients.end());
    }
}

void FakeWebSocketServer::processTextMessageInternal(const QString &message)
{
    auto client = qobject_cast<QWebSocket *>(sender());
    emit processTextMessage(client, message);
}

void FakeWebSocketServer::onNewConnection()
{
    qCInfo(lcFakeWebSocketServer) << "New connection on fake websocket server";

    auto socket = _webSocketServer->nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &FakeWebSocketServer::processTextMessageInternal);
    connect(socket, &QWebSocket::disconnected, this, &FakeWebSocketServer::socketDisconnected);

    _clients << socket;
}

void FakeWebSocketServer::socketDisconnected()
{
    qCInfo(lcFakeWebSocketServer) << "Socket disconnected";

    auto client = qobject_cast<QWebSocket *>(sender());

    if (client) {
        _clients.removeAll(client);
        client->deleteLater();
    }
}

bool FakeWebSocketServer::waitForTextMessages() const
{
    return _processTextMessageSpy->wait();
}

uint32_t FakeWebSocketServer::textMessagesCount() const
{
    return _processTextMessageSpy->count();
}

QString FakeWebSocketServer::textMessage(int messageNumber) const
{
    Q_ASSERT(0 <= messageNumber && messageNumber < _processTextMessageSpy->count());
    return _processTextMessageSpy->at(messageNumber).at(1).toString();
}

QWebSocket *FakeWebSocketServer::socketForTextMessage(int messageNumber) const
{
    Q_ASSERT(0 <= messageNumber && messageNumber < _processTextMessageSpy->count());
    return _processTextMessageSpy->at(messageNumber).at(0).value<QWebSocket *>();
}

void FakeWebSocketServer::clearTextMessages()
{
    _processTextMessageSpy->clear();
}

OCC::AccountPtr FakeWebSocketServer::createAccount(const QString &username, const QString &password)
{
    auto account = OCC::Account::create();

    QStringList typeList;
    typeList.append("files");
    typeList.append("activities");
    typeList.append("notifications");

    QString websocketUrl("ws://localhost:12345");

    QVariantMap endpointsMap;
    endpointsMap["websocket"] = websocketUrl;

    QVariantMap notifyPushMap;
    notifyPushMap["type"] = typeList;
    notifyPushMap["endpoints"] = endpointsMap;

    QVariantMap capabilitiesMap;
    capabilitiesMap["notify_push"] = notifyPushMap;

    account->setCapabilities(capabilitiesMap);

    auto credentials = new CredentialsStub(username, password);
    account->setCredentials(credentials);

    return account;
}

CredentialsStub::CredentialsStub(const QString &user, const QString &password)
    : _user(user)
    , _password(password)
{
}

QString CredentialsStub::authType() const
{
    return "";
}

QString CredentialsStub::user() const
{
    return _user;
}

QString CredentialsStub::password() const
{
    return _password;
}

QNetworkAccessManager *CredentialsStub::createQNAM() const
{
    return nullptr;
}

bool CredentialsStub::ready() const
{
    return false;
}

void CredentialsStub::fetchFromKeychain() { }

void CredentialsStub::askFromUser() { }

bool CredentialsStub::stillValid(QNetworkReply * /*reply*/)
{
    return false;
}

void CredentialsStub::persist() { }

void CredentialsStub::invalidateToken() { }

void CredentialsStub::forgetSensitiveData() { }
