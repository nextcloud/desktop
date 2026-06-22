/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "singleinstancemanager_mac.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(lcSingleInstance, "nextcloud.gui.singleinstance", QtInfoMsg)
}

namespace OCC {

SingleInstanceManager::SingleInstanceManager(QObject *parent)
    : QObject(parent)
{
    const QString path = socketPath();

    // Try to reach a running primary instance.
    QLocalSocket probe;
    probe.connectToServer(path, QLocalSocket::ReadWrite);
    if (probe.waitForConnected(500)) {
        // A live server answered — we are a secondary instance.
        probe.disconnectFromServer();
        _isPrimary = false;
        qCInfo(lcSingleInstance) << "Found running instance at" << path;
        return;
    }

    // No live instance found.  Remove any stale socket file that a previous
    // crash may have left behind, then start listening as the primary.
    QLocalServer::removeServer(path);

    _server = new QLocalServer(this);
    _server->setSocketOptions(QLocalServer::UserAccessOption);

    if (_server->listen(path)) {
        qCInfo(lcSingleInstance) << "Primary instance listening on" << path;
        connect(_server, &QLocalServer::newConnection,
                this, &SingleInstanceManager::onNewConnection);
    } else {
        // listen() failed for an unexpected reason.  Log it but still declare
        // ourselves the primary — it is safer to let the app start than to
        // block it with a spurious "already running" error.
        qCWarning(lcSingleInstance)
            << "Could not listen on" << path << ":" << _server->errorString()
            << "– assuming primary instance";
    }

    _isPrimary = true;
}

SingleInstanceManager::~SingleInstanceManager()
{
    if (_server) {
        QLocalServer::removeServer(socketPath());
    }
}

// static
QString SingleInstanceManager::socketPath()
{
    // In a sandboxed macOS app QDir::homePath() resolves to
    //   ~/Library/Containers/com.nextcloud.desktopclient/Data
    // The App Sandbox grants both file-write and network-bind access to this
    // directory on all macOS versions, without any extra entitlements.
    //
    // Path budget check (worst case: macOS-maximum 31-char username):
    //   /Users/<31 chars>/Library/Containers/com.nextcloud.desktopclient/Data/si
    //   = 7 + 31 + 20 + 27 + 5 + 3 = 93 bytes  (limit: 103)
    return QDir::homePath() + QStringLiteral("/si");
}

bool SingleInstanceManager::isPrimaryInstance() const
{
    return _isPrimary;
}

bool SingleInstanceManager::sendMessage(const QByteArray &data, int timeout)
{
    Q_ASSERT(!_isPrimary); // callers should only send when secondary

    QLocalSocket socket;
    socket.connectToServer(socketPath(), QLocalSocket::ReadWrite);
    if (!socket.waitForConnected(timeout)) {
        qCWarning(lcSingleInstance) << "sendMessage: could not connect to primary instance";
        return false;
    }
    socket.write(data);
    const bool ok = socket.waitForBytesWritten(timeout);
    socket.disconnectFromServer();
    return ok;
}

void SingleInstanceManager::onNewConnection()
{
    while (_server->hasPendingConnections()) {
        QLocalSocket *const conn = _server->nextPendingConnection();
        auto *const buffer = new QByteArray;

        // Accumulate incoming bytes; emit when the sender closes the connection.
        connect(conn, &QLocalSocket::readyRead, this, [conn, buffer] {
            buffer->append(conn->readAll());
        });
        connect(conn, &QLocalSocket::disconnected, this, [this, conn, buffer] {
            if (!buffer->isEmpty()) {
                Q_EMIT messageReceived(*buffer);
            }
            delete buffer;
            conn->deleteLater();
        });
    }
}

} // namespace OCC
