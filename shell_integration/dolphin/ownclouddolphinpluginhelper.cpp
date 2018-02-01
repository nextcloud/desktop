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
#include <QFile>
#include "ownclouddolphinpluginhelper.h"
#include "config.h"

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
    QString runtimeDir = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
    runtimeDir.append( QChar('/'));
    runtimeDir.append( QLatin1String(APPLICATION_SHORTNAME) );

    const QString socketPath = runtimeDir + QLatin1String("/socket");
    _socket.connectToServer(socketPath);
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
        emit commandRecieved(line);
    }
}
