/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "httpserver.h"

HttpServer::HttpServer(quint16 port, QObject* parent)
    : QTcpServer(parent)
{
    listen(QHostAddress::Any, port);
}

void HttpServer::readClient()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    if (socket->canReadLine()) {
        QStringList tokens = QString(socket->readLine()).split(QRegularExpression("[ \r\n][ \r\n]*"));
        if (tokens[0] == "GET") {
            QTextStream os(socket);
            os.setAutoDetectUnicode(true);
            os << "HTTP/1.0 200 Ok\r\n"
                "Content-Type: text/html; charset=\"utf-8\"\r\n"
                "\r\n"
                "<h1>Nothing to see here</h1>\n"
                << QDateTime::currentDateTimeUtc().toString() << "\n";
            socket->close();

            QtServiceBase::instance()->logMessage("Wrote to client");

            if (socket->state() == QTcpSocket::UnconnectedState) {
                delete socket;
                QtServiceBase::instance()->logMessage("Connection closed");
            }
        }
    }
}
void HttpServer::discardClient()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    socket->deleteLater();

    QtServiceBase::instance()->logMessage("Connection closed");
}


void HttpServer::incomingConnection(int socket)
{
    if (disabled)
        return;
    QTcpSocket* s = new QTcpSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);
}
