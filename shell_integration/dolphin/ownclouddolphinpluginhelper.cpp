/******************************************************************************
 *   Copyright (C) 2014 by Olivier Goffart <ogoffart@woboq.com                *
 *                                                                            *
 *   This program is free software; you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published by     *
 *   the Free Software Foundation; either version 2 of the License, or        *
 *   (at your option) any later version.                                      *
 *                                                                            *
 *   This program is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU General Public License for more details.                             *
 *                                                                            *
 *   You should have received a copy of the GNU General Public License        *
 *   along with this program; if not, write to the                            *
 *   Free Software Foundation, Inc.,                                          *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA               *
 ******************************************************************************/

#include <QtNetwork/QLocalSocket>
#include <qcoreevent.h>
#include <QStandardPaths>
#include <QFile>
#include <QLoggingCategory>
#include "ownclouddolphinpluginhelper.h"
#include <QJsonObject>
#include <QJsonDocument>

Q_LOGGING_CATEGORY(lcPluginHelper, "owncloud.dolphin", QtInfoMsg)

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

void OwncloudDolphinPluginHelper::sendGetClientIconCommand(int size)
{
    const QByteArray cmd = QByteArrayLiteral("V2/GET_CLIENT_ICON:");
    const QByteArray newLine = QByteArrayLiteral("\n");
    const QJsonObject args { { QStringLiteral("size"), size } };
    const QJsonObject obj { { QStringLiteral("id"), QString::number(_msgId++) }, { QStringLiteral("arguments"), args } };
    const auto json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendCommand(QByteArray(cmd + json + newLine));
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
                                                APPLICATION_SHORTNAME,
                                                QStandardPaths::LocateDirectory);
    if(socketPath.isEmpty())
        return;

    _socket.connectToServer(socketPath + QLatin1String("/socket"));
}

void OwncloudDolphinPluginHelper::slotReadyRead()
{
    while (_socket.bytesAvailable()) {
        _line += _socket.readLine();
        if (!_line.endsWith("\n")) {
            continue;
        }
        QByteArray line;
        qSwap(line, _line);
        line.chop(1);
        if (line.isEmpty())
            continue;
        const int firstColon = line.indexOf(':');
        if (firstColon == -1) {
            continue;
        }
        // get the command (at begin of line, before first ':')
        const QByteArray command = line.left(firstColon);
        // rest of line contains the information
        const QByteArray info = line.mid(firstColon + 1);

        if (command == QByteArrayLiteral("REGISTER_PATH")) {
            const QString file = QString::fromUtf8(info);
            _paths.append(file);
            continue;
        } else if (command == QByteArrayLiteral("STRING")) {
            auto args = QString::fromUtf8(info).split(':');
            if (args.size() >= 2) {
                _strings[args[0]] = args.mid(1).join(':');
            }
            continue;
        } else if (command == QByteArrayLiteral("VERSION")) {
            auto args = info.split(':');
            if (args.size() >= 2) {
                auto version = args.value(1);
                _version = version;
            }
            if (!_version.startsWith("1.")) {
                // Incompatible version, disconnect forever
                _connectTimer.stop();
                _socket.disconnectFromServer();
                return;
            }
        } else if (command == QByteArrayLiteral("V2/GET_CLIENT_ICON_RESULT")) {
            QJsonParseError error;
            auto json = QJsonDocument::fromJson(info, &error).object();
            if (error.error != QJsonParseError::NoError) {
                qCWarning(lcPluginHelper) << "Error while parsing result: " << error.error;
                continue;
            }

            auto jsonArgs = json.value("arguments").toObject();
            if (jsonArgs.isEmpty()) {
                auto jsonErr = json.value("error").toObject();
                qCWarning(lcPluginHelper) << "Error getting client icon: " << jsonErr;
                continue;
            }

            const QByteArray pngBase64 = jsonArgs.value("png").toString().toUtf8();
            QByteArray png = QByteArray::fromBase64(pngBase64);

            QPixmap pixmap;
            bool isLoaded = pixmap.loadFromData(png, "PNG");
            if (isLoaded) {
                _clientIcon = pixmap;
            }
        }

        emit commandRecieved(line);
    }
}
