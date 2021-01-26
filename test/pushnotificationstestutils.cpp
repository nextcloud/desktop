#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>

#include "pushnotificationstestutils.h"

Q_LOGGING_CATEGORY(lcFakeWebSocketServer, "nextcloud.test.fakewebserver", QtInfoMsg)

FakeWebSocketServer::FakeWebSocketServer(quint16 port, QObject *parent)
    : QObject(parent)
    , _webSocketServer(new QWebSocketServer(QStringLiteral("Fake Server"), QWebSocketServer::NonSecureMode, this))
{
    if (_webSocketServer->listen(QHostAddress::Any, port)) {
        connect(_webSocketServer, &QWebSocketServer::newConnection, this, &FakeWebSocketServer::onNewConnection);
        connect(_webSocketServer, &QWebSocketServer::closed, this, &FakeWebSocketServer::closed);
        qCInfo(lcFakeWebSocketServer) << "Open fake websocket server on port:" << port;
        return;
    }
    Q_UNREACHABLE();
}

FakeWebSocketServer::~FakeWebSocketServer()
{
    close();
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

OCC::AccountPtr FakeWebSocketServer::createAccount()
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
