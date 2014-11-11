/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QApplication>
#include <QLocalSocket>
#include <QDir>

#include "window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QLocalSocket sock;
    QString sockAddr;
#ifdef Q_OS_UNIX
    sockAddr = QDir::homePath() + QLatin1String("/.local/share/data/ownCloud/socket");
#else
    sockAddr = QLatin1String("\\\\.\\pipe\\ownCloud");
#endif
    Window win(&sock);
    QObject::connect(&sock, SIGNAL(readyRead()), &win, SLOT(receive()));
    QObject::connect(&sock, SIGNAL(error(QLocalSocket::LocalSocketError)),
                     &win, SLOT(receiveError(QLocalSocket::LocalSocketError)));

    win.show();
    sock.connectToServer(sockAddr, QIODevice::ReadWrite);
    qDebug() << "Connecting to" << sockAddr;

    return app.exec();
}
