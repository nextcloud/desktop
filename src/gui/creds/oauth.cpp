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
#include <QBuffer>
#include "account.h"
#include "creds/oauth.h"
#include <QJsonObject>
#include <QJsonDocument>
#include "theme.h"
#include "networkjobs.h"
#include "creds/httpcredentials.h"
#include <QRandomGenerator>

namespace OCC {

Q_LOGGING_CATEGORY(lcOauth, "sync.credentials.oauth", QtInfoMsg)

OAuth::~OAuth()
{
}

static void httpReplyAndClose(QTcpSocket *socket, const char *code, const char *html,
    const char *moreHeaders = nullptr)
{
    if (!socket)
        return; // socket can have been deleted if the browser was closed
    socket->write("HTTP/1.1 ");
    socket->write(code);
    socket->write("\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: ");
    socket->write(QByteArray::number(qstrlen(html)));
    if (moreHeaders) {
        socket->write("\r\n");
        socket->write(moreHeaders);
    }
    socket->write("\r\n\r\n");
    socket->write(html);
    socket->disconnectFromHost();
    // We don't want that deleting the server too early prevent queued data to be sent on this socket.
    // The socket will be deleted after disconnection because disconnected is connected to deleteLater
    socket->setParent(nullptr);
}

void OAuth::start()
{
    // Listen on the socket to get a port which will be used in the redirect_uri
    if (!_server.listen(QHostAddress::LocalHost)) {
        emit result(NotSupported, QString());
        return;
    }

    _pkceCodeVerifier = generateRandomString(24);
    ASSERT(_pkceCodeVerifier.size() == 128)
    _state = generateRandomString(8);

    fetchWellKnown();

    openBrowser();

    QObject::connect(&_server, &QTcpServer::newConnection, this, [this] {
        while (QPointer<QTcpSocket> socket = _server.nextPendingConnection()) {
            QObject::connect(socket.data(), &QTcpSocket::disconnected, socket.data(), &QTcpSocket::deleteLater);
            QObject::connect(socket.data(), &QIODevice::readyRead, this, [this, socket] {
                const QByteArray peek = socket->peek(qMin(socket->bytesAvailable(), 4000LL)); //The code should always be within the first 4K
                if (peek.indexOf('\n') < 0)
                    return; // wait until we find a \n
                if (!peek.startsWith("GET /?")) {
                    httpReplyAndClose(socket, "404 Not Found", "<html><head><title>404 Not Found</title></head><body><center><h1>404 Not Found</h1></center></body></html>");
                    return;
                }
                const int offset = 6;
                const QUrlQuery args(peek.mid(offset, peek.indexOf(' ', offset) - offset));
                if (args.queryItemValue(QStringLiteral("state")) != _state) {
                    httpReplyAndClose(socket, "400 Bad Request", "<html><head><title>400 Bad Request</title></head><body><center><h1>400 Bad Request</h1></center></body></html>");
                    return;
                }

                const QUrl requestTokenUrl = _tokenEndpoint.isValid()
                    ? _tokenEndpoint
                    : Utility::concatUrlPath(_account->url(), QLatin1String("/index.php/apps/oauth2/api/v1/token"));

                QNetworkRequest req;
                req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");

                const QString basicAuth = QStringLiteral("%1:%2").arg(
                    Theme::instance()->oauthClientId(), Theme::instance()->oauthClientSecret());
                req.setRawHeader("Authorization", "Basic " + basicAuth.toUtf8().toBase64());
                // We just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
                req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);

                auto requestBody = new QBuffer;
                QUrlQuery arguments;
                arguments.setQueryItems({
                    { QStringLiteral("grant_type"), "authorization_code" },
                    { QStringLiteral("code"), args.queryItemValue(QStringLiteral("code")) },
                    { QStringLiteral("client_id"), Theme::instance()->oauthClientId() },
                    { QStringLiteral("client_secret"), Theme::instance()->oauthClientSecret() },
                    { QStringLiteral("redirect_uri"), QStringLiteral("http://localhost:%1").arg(_server.serverPort()) },
                    { QStringLiteral("code_verifier"), _pkceCodeVerifier },
                    { QStringLiteral("scope"), args.queryItemValue(QStringLiteral("scope")) },
                });
                requestBody->setData(arguments.query(QUrl::FullyEncoded).toUtf8());
                const auto job = _account->sendRequest("POST", requestTokenUrl, req, requestBody);
                job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
                QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this, socket](QNetworkReply *reply) {
                    auto jsonData = reply->readAll();
                    QJsonParseError jsonParseError;
                    QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();
                    QString accessToken = json["access_token"].toString();
                    QString refreshToken = json["refresh_token"].toString();
                    QString user = json["user_id"].toString();
                    QUrl messageUrl = json["message_url"].toString();

                    if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError
                        || jsonData.isEmpty() || json.isEmpty() || refreshToken.isEmpty() || accessToken.isEmpty()
                        || json["token_type"].toString() != QLatin1String("Bearer")) {
                        QString errorReason;
                        QString errorFromJson = json["error_description"].toString();
                        if (errorFromJson.isEmpty())
                            errorFromJson = json["error"].toString();
                        if (!errorFromJson.isEmpty()) {
                            errorReason = tr("Error returned from the server: <em>%1</em>")
                                              .arg(errorFromJson.toHtmlEscaped());
                        } else if (reply->error() != QNetworkReply::NoError) {
                            errorReason = tr("There was an error accessing the 'token' endpoint: <br><em>%1</em>")
                                              .arg(reply->errorString().toHtmlEscaped());
                        } else if (jsonData.isEmpty()) {
                            // Can happen if a funky load balancer strips away POST data, e.g. BigIP APM my.policy
                            errorReason = tr("Empty JSON from OAuth2 redirect");
                            // We explicitly have this as error case since the json qcWarning output below is misleading,
                            // it will show a fake json will null values that actually never was received like this as
                            // soon as you access json["whatever"] the debug output json will claim to have "whatever":null
                        } else if (jsonParseError.error != QJsonParseError::NoError) {
                            errorReason = tr("Could not parse the JSON returned from the server: <br><em>%1</em>")
                                              .arg(jsonParseError.errorString());
                        } else {
                            errorReason = tr("The reply from the server did not contain all expected fields");
                        }
                        qCWarning(lcOauth) << "Error when getting the accessToken" << jsonData << errorReason;
                        httpReplyAndClose(socket, "500 Internal Server Error",
                            tr("<h1>Login Error</h1><p>%1</p>").arg(errorReason).toUtf8().constData());
                        emit result(Error);
                        return;
                    }
                    if (!user.isEmpty()) {
                        finalize(socket, accessToken, refreshToken, user, messageUrl);
                        return;
                    }
                    // If the reply don't contains the user id, we must do another call to query it
                    JsonApiJob *job = new JsonApiJob(_account->sharedFromThis(), QLatin1String("ocs/v1.php/cloud/user"), this);
                    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
                    QNetworkRequest req;
                    // We are not connected yet so we need to handle the authentication manually
                    req.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
                    // We just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
                    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
                    job->startWithRequest(req);
                    QObject::connect(job, &JsonApiJob::jsonReceived, this, [=](const QJsonDocument &json) {
                        QString user = json.object().value("ocs").toObject().value("data").toObject().value("id").toString();
                        finalize(socket, accessToken, refreshToken, user, messageUrl);
                    });
                });
            });
        }
    });
}

void OAuth::finalize(QPointer<QTcpSocket> socket, const QString &accessToken,
                     const QString &refreshToken, const QString &user, const QUrl &messageUrl) {
    if (!_expectedUser.isNull() && user != _expectedUser) {
        // Connected with the wrong user
        QString message = tr("<h1>Wrong user</h1>"
                                "<p>You logged-in with user <em>%1</em>, but must login with user <em>%2</em>.<br>"
                                "Please log out of %3 in another tab, then <a href='%4'>click here</a> "
                                "and log in as user %2</p>")
                                .arg(user, _expectedUser, Theme::instance()->appNameGUI(),
                                    authorisationLink().toString(QUrl::FullyEncoded));
        httpReplyAndClose(socket, "200 OK", message.toUtf8().constData());
        // We are still listening on the socket so we will get the new connection
        return;
    }
    const char *loginSuccessfullHtml = "<h1>Login Successful</h1><p>You can close this window.</p>";
    if (messageUrl.isValid()) {
        httpReplyAndClose(socket, "303 See Other", loginSuccessfullHtml,
            QByteArray("Location: " + messageUrl.toEncoded()).constData());
    } else {
        httpReplyAndClose(socket, "200 OK", loginSuccessfullHtml);
    }
    emit result(LoggedIn, user, accessToken, refreshToken);
}

QByteArray OAuth::generateRandomString(size_t size) const
{
    // TODO: do we need a varaible size?
    std::vector<quint32> buffer(size, 0);
    QRandomGenerator::global()->fillRange(buffer.data(), static_cast<qsizetype>(size));
    return QByteArray(reinterpret_cast<char *>(buffer.data()), static_cast<int>(size * sizeof(quint32))).toBase64(QByteArray::Base64UrlEncoding);
}

QUrl OAuth::authorisationLink() const
{
    Q_ASSERT(_server.isListening());
    QUrlQuery query;
    QByteArray code_challenge = QCryptographicHash::hash(_pkceCodeVerifier, QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    query.setQueryItems({
        { QLatin1String("response_type"), QLatin1String("code") },
        { QLatin1String("client_id"), Theme::instance()->oauthClientId() },
        { QLatin1String("redirect_uri"), QLatin1String("http://localhost:") + QString::number(_server.serverPort()) },
        { QLatin1String("code_challenge"), QString::fromLatin1(code_challenge) },
        { QLatin1String("code_challenge_method"), QLatin1String("S256") },
        { QLatin1String("scope"), QLatin1String("openid offline_access") },
        { QLatin1String("prompt"), QLatin1String("consent") },
        { QStringLiteral("state"), _state },
    });
    if (!_expectedUser.isNull())
        query.addQueryItem("user", _expectedUser);
    const QUrl url = _authEndpoint.isValid()
        ? Utility::concatUrlPath(_authEndpoint, {}, query)
        : Utility::concatUrlPath(_account->url(), QLatin1String("/index.php/apps/oauth2/authorize"), query);
    return url;
}

void OAuth::authorisationLinkAsync(std::function<void (const QUrl &)> callback) const
{
    if (_wellKnownFinished) {
        callback(authorisationLink());
    } else {
        connect(this, &OAuth::authorisationLinkChanged, callback);
    }
}

void OAuth::fetchWellKnown()
{
    QUrl wellKnownUrl = Utility::concatUrlPath(_account->url().toString(), QLatin1String("/.well-known/openid-configuration"));
    QNetworkRequest req;
    auto job = _account->sendRequest("GET", wellKnownUrl);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
    QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        _wellKnownFinished = true;
        if (reply->error() != QNetworkReply::NoError) {
            // Most likely the file does not exist, default to the normal endpoint
            emit this->authorisationLinkChanged(authorisationLink());
            return;
        }
        const auto jsonData = reply->readAll();
        QJsonParseError jsonParseError;
        const QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();

        if (jsonParseError.error == QJsonParseError::NoError) {
            QString authEp = json["authorization_endpoint"].toString();
            if (!authEp.isEmpty())
                this->_authEndpoint = authEp;
            QString tokenEp = json["token_endpoint"].toString();
            if (!tokenEp.isEmpty())
                this->_tokenEndpoint = tokenEp;
        } else if (jsonParseError.error == QJsonParseError::IllegalValue) {
            qCDebug(lcOauth) << ".well-known did not return json, the server most does not support oidc";
        } else {
            qCWarning(lcOauth) << "Json parse error in well-known: " << jsonParseError.errorString();
        }

        emit this->authorisationLinkChanged(authorisationLink());
    });
}

void OAuth::openBrowser()
{
    authorisationLinkAsync([this](const QUrl &link) {
        if (!QDesktopServices::openUrl(link)) {
            qCWarning(lcOauth) << "QDesktopServices::openUrl Failed";
            // We cannot open the browser, then we claim we don't support OAuth.
            emit result(NotSupported, QString());
        }
    });
}

} // namespace OCC
