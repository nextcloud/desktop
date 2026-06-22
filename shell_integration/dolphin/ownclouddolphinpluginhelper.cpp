/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtNetwork/QLocalSocket>
#include <qcoreevent.h>
#include <QStandardPaths>
#include <QFile>
#include "ownclouddolphinpluginhelper.h"

OwncloudDolphinPluginHelper* OwncloudDolphinPluginHelper::instance()
{
    static OwncloudDolphinPluginHelper self;
    return &self;
}

OwncloudDolphinPluginHelper::OwncloudDolphinPluginHelper()
{
    connect(&_socket, &QLocalSocket::connected, this, &OwncloudDolphinPluginHelper::slotConnected);
    connect(&_socket, &QLocalSocket::readyRead, this, &OwncloudDolphinPluginHelper::slotReadyRead);
    _connectTimer.start(45 * 1000, Qt::VeryCoarseTimer, this);
    tryConnect();
}

void OwncloudDolphinPluginHelper::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == _connectTimer.timerId()) {
        tryConnect();
        return;
    }
    QObject::timerEvent(e);
}

bool OwncloudDolphinPluginHelper::isConnected() const
{
    return _socket.state() == QLocalSocket::ConnectedState;
}

void OwncloudDolphinPluginHelper::sendCommand(const char* data)
{
    _socket.write(data);
    _socket.flush();
}

void OwncloudDolphinPluginHelper::slotConnected()
{
    sendCommand("VERSION:\n");
    sendCommand("GET_STRINGS:\n");
}

void OwncloudDolphinPluginHelper::tryConnect()
{
    if (_socket.state() != QLocalSocket::UnconnectedState) {
        return;
    }
    
    QString socketPath = QStandardPaths::locate(QStandardPaths::RuntimeLocation,
                                                QStringLiteral(APPLICATION_SHORTNAME),
                                                QStandardPaths::LocateDirectory);
    if(socketPath.isEmpty())
        return;

    _socket.connectToServer(socketPath + QLatin1String("/socket"));
}

void OwncloudDolphinPluginHelper::slotReadyRead()
{
    while (_socket.bytesAvailable()) {
        _line += _socket.readLine();
        if (!_line.endsWith("\n"))
            continue;
        QByteArray line;
        qSwap(line, _line);
        line.chop(1);
        if (line.isEmpty())
            continue;

        if (line.startsWith("REGISTER_PATH:")) {
            auto col = line.indexOf(':');
            QString file = QString::fromUtf8(line.constData() + col + 1, line.size() - col - 1);
            _paths.append(file);
            continue;
        } else if (line.startsWith("STRING:")) {
            auto args = QString::fromUtf8(line).split(QLatin1Char(':'));
            if (args.size() >= 3) {
                _strings[args[1]] = args.mid(2).join(QLatin1Char(':'));
            }
            continue;
        } else if (line.startsWith("VERSION:")) {
            auto args = line.split(':');
            auto version = args.value(2);
            _version = version;
            if (!version.startsWith("1.")) {
                // Incompatible version, disconnect forever
                _connectTimer.stop();
                _socket.disconnectFromServer();
                return;
            }
        }
        Q_EMIT commandRecieved(line);
    }
}
