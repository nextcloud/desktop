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

#include "socketclient.h"

SocketClient::SocketClient(QObject *parent) :
    QObject(parent)
  , sock(new QLocalSocket(this))
{
    QString sockAddr;
#ifdef Q_OS_UNIX
    sockAddr = QDir::homePath() + QLatin1String("/.local/share/data/ownCloud/");
#else
    sockAddr = QLatin1String("\\\\.\\pipe\\ownCloud");
#endif
    sock->connectToServer(sockAddr);
    sock->open(QIODevice::ReadWrite);
    connect(sock, SIGNAL(readyRead()), SLOT(writeData()));
}

void SocketClient::writeData()
{
    qDebug() << sock->readAll();
}
