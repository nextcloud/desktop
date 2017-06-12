/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QDesktopServices>
#include <QNetworkReply>
#include <QTimer>
#include "account.h"
#include "creds/oauth.h"
#include <QJsonObject>
#include <QJsonDocument>
#include "theme.h"


namespace OCC {

OAuth::~OAuth()
{
}

static void httpReplyAndClose(QTcpSocket *socket, const char *code, const char *html)
{
    socket->write("HTTP/1.1 ");
    socket->write(code);
    socket->write("\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ");
    socket->write(QByteArray::number(qstrlen(html)));
    socket->write("\r\n\r\n");
    socket->write(html);
    socket->disconnectFromHost();
}

void OAuth::start()
{
    // Listen on the socket to get a port which will be used in the redirect_uri
    if (!_server.listen(QHostAddress::LocalHost)) {
        emit result(NotSupported, QString());
        return;
    }

    if (!openBrowser())
        return;

    QObject::connect(&_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *socket = _server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
            QObject::connect(socket, &QIODevice::readyRead, this, [this, socket] {
                QByteArray peek = socket->peek(qMin(socket->bytesAvailable(), 4000LL)); //The code should always be within the first 4K
                if (peek.indexOf('\n') < 0)
                    return; // wait until we find a \n
                QRegExp rx("^GET /\\?code=([a-zA-Z0-9]+)[& ]"); // Match a  /?code=...  URL
                if (rx.indexIn(peek) != 0) {
                    httpReplyAndClose(socket, "404 Not Found", "<html><head><title>404 Not Found</title></head><body><center><h1>404 Not Found</h1></center></body></html>");
                    return;
                }

                // TODO: add redirect to the page on the server
                httpReplyAndClose(socket, "200 OK", "<h1>Login Successful</h1><p>You can close this window.</p>");

                QString code = rx.cap(1); // The 'code' is the first capture of the regexp

                QUrl requestToken(_account->url().toString()
                    + QLatin1String("/index.php/apps/oauth2/api/v1/token?grant_type=authorization_code&code=")
                    + code
                    + QLatin1String("&redirect_uri=http://localhost:") + QString::number(_server.serverPort()));
                requestToken.setUserName(Theme::instance()->oauthClientId());
                requestToken.setPassword(Theme::instance()->oauthClientSecret());
                QNetworkRequest req;
                req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
                auto reply = _account->sendRequest("POST", requestToken, req);
                QTimer::singleShot(30 * 1000, reply, &QNetworkReply::abort);
                QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] {
                    auto jsonData = reply->readAll();
                    QJsonParseError jsonParseError;
                    QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();
                    QString accessToken = json["access_token"].toString();
                    QString refreshToken = json["refresh_token"].toString();
                    QString user = json["user_id"].toString();

                    if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError
                        || json.isEmpty() || refreshToken.isEmpty() || accessToken.isEmpty()
                        || json["token_type"].toString() != QLatin1String("Bearer")) {
                        qDebug() << "Error when getting the accessToken" << reply->error() << json << jsonParseError.errorString();
                        emit result(Error);
                        return;
                    }
                    emit result(LoggedIn, user, accessToken, refreshToken);
                });
            });
        }
    });
    QTimer::singleShot(5 * 60 * 1000, this, [this] { result(Error); });
}


bool OAuth::openBrowser()
{
    Q_ASSERT(_server.isListening());
    auto url = QUrl(_account->url().toString()
        + QLatin1String("/index.php/apps/oauth2/authorize?response_type=code&client_id=")
        + Theme::instance()->oauthClientId()
        + QLatin1String("&redirect_uri=http://localhost:") + QString::number(_server.serverPort()));


    if (!QDesktopServices::openUrl(url)) {
        // We cannot open the browser, then we claim we don't support OAuth.
        emit result(NotSupported, QString());
        return false;
    }
    return true;
}

} // namespace OCC
