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
#include <networkjobs.h>
#include "account.h"
#include "credentialmanager.h"
#include "creds/oauth.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>

#include "theme.h"
#include "networkjobs.h"
#include "creds/httpcredentials.h"

using namespace OCC;
Q_LOGGING_CATEGORY(lcOauth, "sync.credentials.oauth", QtInfoMsg)

namespace {
const QString clientSecretC()
{
    return QStringLiteral("clientSecret");
}

QVariant getRequiredField(const QVariantMap &json, const QString &s, QString *error)
{
    const auto out = json.constFind(s);
    if (out == json.constEnd()) {
        error->append(QStringLiteral("\tError: Missing field %1\n").arg(s));
        return {};
    }
    return *out;
}

void httpReplyAndClose(const QPointer<QTcpSocket> &socket, const QByteArray &code, const QByteArray &html,
    const QByteArray &moreHeaders = {})
{
    if (!socket)
        return; // socket can have been deleted if the browser was closed
    // clang format has issues with the next line...
    const QByteArray msg = QByteArrayLiteral("HTTP/1.1 ") % code % QByteArrayLiteral("\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: ") % QByteArray::number(html.length()) % (!moreHeaders.isEmpty() ? QByteArrayLiteral("\r\n") % moreHeaders : QByteArray()) % QByteArrayLiteral("\r\n\r\n") % html;
    qCDebug(lcOauth) << msg;
    socket->write(msg);
    socket->disconnectFromHost();
    // We don't want that deleting the server too early prevent queued data to be sent on this socket.
    // The socket will be deleted after disconnection because disconnected is connected to deleteLater
    socket->setParent(nullptr);
}

class RegisterClientJob : public QObject
{
    Q_OBJECT
public:
    RegisterClientJob(Account *accout, const QUrl &registrationEndpoint, QObject *parent)
        : QObject(parent)
        , _account(accout)
        , _registrationEndpoint(registrationEndpoint)
    {
        connect(this, &RegisterClientJob::errorOccured, this, &RegisterClientJob::deleteLater);
        connect(this, &RegisterClientJob::finished, this, &RegisterClientJob::deleteLater);
    }

    void start()
    {
        auto job = _account->credentialManager()->get(clientSecretC());
        connect(job, &CredentialJob::finished, this, [this, job] {
            qDebug() << job->errorString() << job->error();
            if (job->data().isValid()) {
                rgisterClientFinished(job->data().value<QVariantMap>());
            } else {
                qCCritical(lcOauth) << "Failed to read client id" << job->error();
                rgisterClientOnline();
            }
        });
    }

Q_SIGNALS:
    void finished(const QString &clientId, const QString &clientSecret);
    void errorOccured(const QString &error);

private:
    void rgisterClientOnline()
    {
        auto job = new OCC::SimpleNetworkJob(_account->sharedFromThis(), this);
        job->setAuthenticationJob(true);
        connect(job, &OCC::SimpleNetworkJob::finishedSignal, this, [this, job] {
            const auto data = job->reply()->readAll();
            QJsonParseError error;
            const auto json = QJsonDocument::fromJson(data, &error);
            if (error.error == QJsonParseError::NoError) {
                rgisterClientFinished(json.object().toVariantMap());
            } else {
                qCWarning(lcOauth) << "Failed to register the client" << error.errorString() << data;
                Q_EMIT errorOccured(error.errorString());
            }
        });
        const QJsonObject json({ { QStringLiteral("client_name"), QStringLiteral("%1 %2").arg(Theme::instance()->appNameGUI(), Theme::instance()->version()) },
            { QStringLiteral("redirect_uris"), QJsonArray { QStringLiteral("http://localhost") } },
            { QStringLiteral("application_type"), QStringLiteral("native") },
            { QStringLiteral("token_endpoint_auth_method"), QStringLiteral("client_secret_basic") } });
        job->prepareRequest("POST", _registrationEndpoint, QNetworkRequest(), json);
        job->start();
    }

    void rgisterClientFinished(const QVariantMap &data)
    {
        {
            QString error;
            auto expireDate = QDateTime::fromSecsSinceEpoch(getRequiredField(data, QStringLiteral("client_secret_expires_at"), &error).value<qint64>());
            if (!error.isEmpty()) {
                Q_EMIT errorOccured(error);
                return;
            }
            qCInfo(lcOauth) << "Client id iessued at:" << QDateTime::fromSecsSinceEpoch(data[QStringLiteral("client_id_issued_at")].value<quint64>())
                            << "expires at" << expireDate;
            if (QDateTime::currentDateTimeUtc() > expireDate) {
                qCDebug(lcOauth) << "Client registration expired";
                rgisterClientOnline();
                return;
            }
        }
        _account->credentialManager()->set(clientSecretC(), data);
        QString error;
        const auto client_id = getRequiredField(data, QStringLiteral("client_id"), &error).toString();
        const auto client_secret = getRequiredField(data, QStringLiteral("client_secret"), &error).toString();
        if (!error.isEmpty()) {
            Q_EMIT errorOccured(error);
            return;
        }
        Q_EMIT finished(client_id, client_secret);
    }

private:
    Account *_account;
    QUrl _registrationEndpoint;
};

}

OAuth::OAuth(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _clientId(Theme::instance()->oauthClientId())
    , _clientSecret(Theme::instance()->oauthClientSecret())
{
}

OAuth::~OAuth()
{
}

void OAuth::startAuthentication()
{
    // Listen on the socket to get a port which will be used in the redirect_uri
    if (!_server.listen(QHostAddress::LocalHost)) {
        emit result(NotSupported, QString());
        return;
    }

    _pkceCodeVerifier = generateRandomString(24);
    OC_ASSERT(_pkceCodeVerifier.size() == 128)
    _state = generateRandomString(8);

    connect(this, &OAuth::fetchWellKnownFinished, this, [this] {
        if (_registrationEndpoint.isValid()) {
            auto job = new RegisterClientJob(_account, _registrationEndpoint, this);
            connect(job, &RegisterClientJob::finished, this, [this](const QString &clientId, const QString &clientSecret) {
                _clientId = clientId;
                _clientSecret = clientSecret;
                Q_EMIT authorisationLinkChanged(authorisationLink());
            });
            connect(job, &RegisterClientJob::errorOccured, this, [this](const QString &error) {
                qCWarning(lcOauth) << "Failed to dynamically register the client, try the default client id" << error;
                Q_EMIT authorisationLinkChanged(authorisationLink());
            });
            job->start();
        } else {
            Q_EMIT authorisationLinkChanged(authorisationLink());
        }
    });
    fetchWellKnown();

    openBrowser();

    QObject::connect(&_server, &QTcpServer::newConnection, this, [this] {
        while (QPointer<QTcpSocket> socket = _server.nextPendingConnection()) {
            QObject::connect(socket.data(), &QTcpSocket::disconnected, socket.data(), &QTcpSocket::deleteLater);
            QObject::connect(socket.data(), &QIODevice::readyRead, this, [this, socket] {
                const QByteArray peek = socket->peek(qMin(socket->bytesAvailable(), 4000LL)); //The code should always be within the first 4K
                if (!peek.contains('\n'))
                    return; // wait until we find a \n
                qCDebug(lcOauth) << "Server provided:" << peek;
                const auto getPrefix = QByteArrayLiteral("GET /?");
                if (!peek.startsWith(getPrefix)) {
                    httpReplyAndClose(socket, QByteArrayLiteral("404 Not Found"), QByteArrayLiteral("<html><head><title>404 Not Found</title></head><body><center><h1>404 Not Found</h1></center></body></html>"));
                    return;
                }
                const auto endOfUrl = peek.indexOf(' ', getPrefix.length());
                const QUrlQuery args(QUrl::fromPercentEncoding(peek.mid(getPrefix.length(), endOfUrl - getPrefix.length())));
                if (args.queryItemValue(QStringLiteral("state")).toUtf8() != _state) {
                    httpReplyAndClose(socket, QByteArrayLiteral("400 Bad Request"), QByteArrayLiteral("<html><head><title>400 Bad Request</title></head><body><center><h1>400 Bad Request</h1></center></body></html>"));
                    return;
                }
              auto job = postTokenRequest({
                    { QStringLiteral("grant_type"), QStringLiteral("authorization_code") },
                    { QStringLiteral("code"), args.queryItemValue(QStringLiteral("code")) },
                    { QStringLiteral("redirect_uri"), QStringLiteral("http://localhost:%1").arg(_server.serverPort()) },
                    { QStringLiteral("code_verifier"), QString::fromUtf8(_pkceCodeVerifier) },
                    });
                QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this, socket](QNetworkReply *reply) {
                    const auto jsonData = reply->readAll();
                    QJsonParseError jsonParseError;
                    const auto data = QJsonDocument::fromJson(jsonData, &jsonParseError).object().toVariantMap();
                    QString fieldsError;
                    const QString accessToken = getRequiredField(data, QStringLiteral("access_token"), &fieldsError).toString();
                    const QString refreshToken = getRequiredField(data, QStringLiteral("refresh_token"), &fieldsError).toString();
                    const QString tokenType = getRequiredField(data, QStringLiteral("token_type"), &fieldsError).toString().toLower();
                    const QString user = data[QStringLiteral("user_id")].toString();
                    const QUrl messageUrl = QUrl::fromEncoded(data[QStringLiteral("message_url")].toByteArray());

                    if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError
                        || !fieldsError.isEmpty()
                        || tokenType != QLatin1String("bearer")) {
                        // do we have error message suitable for users?
                        QString errorReason = data[QStringLiteral("error_description")].toString();
                        if (errorReason.isEmpty()) {
                            // fall back to technical error
                            errorReason = data[QStringLiteral("error")].toString();
                        }
                        if (!errorReason.isEmpty()) {
                            errorReason = tr("Error returned from the server: <em>%1</em>")
                                              .arg(errorReason.toHtmlEscaped());
                        } else if (reply->error() != QNetworkReply::NoError) {
                            errorReason = tr("There was an error accessing the 'token' endpoint: <br><em>%1</em>")
                                              .arg(reply->errorString().toHtmlEscaped());
                        } else if (jsonParseError.error != QJsonParseError::NoError) {
                            errorReason = tr("Could not parse the JSON returned from the server: <br><em>%1</em>")
                                              .arg(jsonParseError.errorString());
                        } else if (tokenType != QStringLiteral("bearer")) {
                            errorReason = tr("Unsupported token type: %1").arg(tokenType);
                        } else if (!fieldsError.isEmpty()) {
                            errorReason = tr("The reply from the server did not contain all expected fields\n:%1").arg(fieldsError);
                        } else {
                            errorReason = tr("Unknown Error");
                        }
                        qCWarning(lcOauth) << "Error when getting the accessToken" << errorReason << "received data:" << jsonData;
                        httpReplyAndClose(socket, QByteArrayLiteral("500 Internal Server Error"),
                            tr("<h1>Login Error</h1><p>%1</p>").arg(errorReason).toUtf8());
                        emit result(Error);
                        return;
                    }
                    if (!user.isEmpty()) {
                        finalize(socket, accessToken, refreshToken, user, messageUrl);
                        return;
                    }
                    // If the reply don't contains the user id, we must do another call to query it
                    JsonApiJob *job = new JsonApiJob(_account->sharedFromThis(), QStringLiteral("ocs/v2.php/cloud/user"), this);
                    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
                    QNetworkRequest req;
                    // We are not connected yet so we need to handle the authentication manually
                    req.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
                    // We just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
                    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
                    job->startWithRequest(req);
                    connect(job, &JsonApiJob::jsonReceived, this, [=](const QJsonDocument &json, int status) {
                        if (status != 200) {
                            httpReplyAndClose(socket, QByteArrayLiteral("500 Internal Server Error"),
                                tr("<h1>Login Error</h1><p>Failed to retrieve user info</p>").toUtf8());
                            emit result(Error);
                        } else {
                            const QString user = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toString();
                            finalize(socket, accessToken, refreshToken, user, messageUrl);
                        }
                    });
                });
                job->start();
            });
        }
    });
}

void OAuth::refreshAuthentication(const QString &refreshToken)
{
    auto refresh = [this, &refreshToken] {
        auto job = postTokenRequest({ { QStringLiteral("grant_type"), QStringLiteral("refresh_token") },
            { QStringLiteral("refresh_token"), refreshToken } });
        connect(job, &SimpleNetworkJob::finishedSignal, this, [this, refreshToken](QNetworkReply *reply) {
            const auto jsonData = reply->readAll();
            QString accessToken;
            QString newRefreshToken = refreshToken;
            QJsonParseError jsonParseError;
            // https://developer.okta.com/docs/reference/api/oidc/#response-properties-2
            const auto data = QJsonDocument::fromJson(jsonData, &jsonParseError).object().toVariantMap();
            const QString error = data.value(QStringLiteral("error")).toString();
            if (!error.isEmpty()) {
                if (error == QLatin1String("invalid_grant") ||
                    error == QLatin1String("invalid_request")) {
                    newRefreshToken.clear();
                } else {
                    qCWarning(lcOauth) << tr("Error while refreshing the token: %1 : %2").arg(error, data.value(QStringLiteral("error_description")).toString());
                }
            } else if (reply->error() != QNetworkReply::NoError) {
                qCWarning(lcOauth) << tr("Error while refreshing the token: %1 : %2").arg(reply->errorString(), QString::fromUtf8(jsonData));
            } else {
                if (jsonParseError.error != QJsonParseError::NoError || data.isEmpty()) {
                    // Invalid or empty JSON: Network error maybe?
                    qCWarning(lcOauth) << tr("Error while refreshing the token: %1 : %2").arg(jsonParseError.errorString(), QString::fromUtf8(jsonData));
                } else {
                    QString error;
                    accessToken = getRequiredField(data, QStringLiteral("access_token"), &error).toString();
                    if (!error.isEmpty()) {
                        qCWarning(lcOauth) << tr("The reply from the server did not contain all expected fields\n:%1\nReceived data: %2").arg(error, QString::fromUtf8(jsonData));
                    }

                    const auto refresh_token = data.find(QStringLiteral("refresh_token"));
                    if (refresh_token != data.constEnd()) {
                        newRefreshToken = refresh_token.value().toString();
                    }
                }
            }
            Q_EMIT refreshFinished(accessToken, newRefreshToken);
        });
        job->start();
    };
    connect(this, &OAuth::fetchWellKnownFinished, this, [this, refresh] {
        if (_registrationEndpoint.isValid()) {
            auto registerJob = new RegisterClientJob(_account, _registrationEndpoint, this);
            connect(registerJob, &RegisterClientJob::finished, this, [this, refresh](const QString &clientId, const QString &clientSecret) {
                _clientId = clientId;
                _clientSecret = clientSecret;
                refresh();
            });
            connect(registerJob, &RegisterClientJob::errorOccured, this, [this](const QString &error) {
                qCWarning(lcOauth) << "Failed to dynamically register the client, try the default client id" << error;
                Q_EMIT refreshFinished(QString(), QString());
            });
            registerJob->start();
        } else {
            refresh();
        }
    });
    fetchWellKnown();
}

void OAuth::finalize(const QPointer<QTcpSocket> &socket, const QString &accessToken,
    const QString &refreshToken, const QString &user, const QUrl &messageUrl)
{
    if (!_account->davUser().isNull() && user != _account->davUser()) {
        // Connected with the wrong user
        qCWarning(lcOauth) << "We expected the user" << _account->davUser() << "but the server answered with user" << user;
        const QString message = tr("<h1>Wrong user</h1>"
                                   "<p>You logged-in with user <em>%1</em>, but must login with user <em>%2</em>.<br>"
                                   "Please log out of %3 in another tab, then <a href='%4'>click here</a> "
                                   "and log in as user %2</p>")
                                    .arg(user, _account->davUser(), Theme::instance()->appNameGUI(),
                                        authorisationLink().toString(QUrl::FullyEncoded));
        httpReplyAndClose(socket, QByteArrayLiteral("403 Forbidden"), message.toUtf8());
        // We are still listening on the socket so we will get the new connection
        return;
    }
    const auto loginSuccessfullHtml = QByteArrayLiteral("<h1>Login Successful</h1><p>You can close this window.</p>");
    if (messageUrl.isValid()) {
        httpReplyAndClose(socket, QByteArrayLiteral("303 See Other"), loginSuccessfullHtml,
            QByteArrayLiteral("Location: ") + messageUrl.toEncoded());
    } else {
        httpReplyAndClose(socket, QByteArrayLiteral("200 OK"), loginSuccessfullHtml);
    }
    emit result(LoggedIn, user, accessToken, refreshToken);
}

SimpleNetworkJob *OAuth::postTokenRequest(const QList<QPair<QString, QString>> &queryItems)
{
    const QUrl requestTokenUrl = _tokenEndpoint.isEmpty() ? Utility::concatUrlPath(_account->url(), QStringLiteral("/index.php/apps/oauth2/api/v1/token")) : _tokenEndpoint;
    QNetworkRequest req;
    const QByteArray basicAuth = QStringLiteral("%1:%2").arg(_clientId, _clientSecret).toUtf8().toBase64();
    req.setRawHeader("Authorization", "Basic " + basicAuth);
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);

    QUrlQuery arguments;
    arguments.setQueryItems(QList<QPair<QString, QString>> { { QStringLiteral("client_id"), _clientId },
                                { QStringLiteral("client_secret"), _clientSecret },
                                { QStringLiteral("scope"), Theme::instance()->openIdConnectScopes() } }
        << queryItems);
    auto job = new SimpleNetworkJob(_account->sharedFromThis(), this);
    job->setAuthenticationJob(true);
    job->prepareRequest("POST", requestTokenUrl, req, arguments);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
    return job;
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
    const QByteArray code_challenge = QCryptographicHash::hash(_pkceCodeVerifier, QCryptographicHash::Sha256)
                                          .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    query.setQueryItems({ { QStringLiteral("response_type"), QStringLiteral("code") },
        { QStringLiteral("client_id"), _clientId },
        { QStringLiteral("redirect_uri"), QStringLiteral("http://localhost:%1").arg(QString::number(_server.serverPort())) },
        { QStringLiteral("code_challenge"), QString::fromLatin1(code_challenge) },
        { QStringLiteral("code_challenge_method"), QStringLiteral("S256") },
        { QStringLiteral("scope"), Theme::instance()->openIdConnectScopes() },
        { QStringLiteral("prompt"), Theme::instance()->openIdConnectPrompt() },
        { QStringLiteral("state"), QString::fromUtf8(_state) } });

    if (!_account->davUser().isNull()) {
        const QString davUser = _account->davUser().replace(QLatin1Char('+'), QStringLiteral("%2B")); // Issue #7762;
        // open id connect
        query.addQueryItem(QStringLiteral("login_hint"), davUser);
        // oc 10
        query.addQueryItem(QStringLiteral("user"), davUser);
    }
    const QUrl url = _authEndpoint.isValid()
        ? Utility::concatUrlPath(_authEndpoint, {}, query)
        : Utility::concatUrlPath(_account->url(), QStringLiteral("/index.php/apps/oauth2/authorize"), query);
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
    auto checkServer = new CheckServerJob(_account->sharedFromThis(), this);
    connect(checkServer, &CheckServerJob::instanceNotFound, this, [this] {
        // TODO: implement proper error signal
        Q_EMIT refreshFinished(QString(), QString());
    });
    connect(checkServer, &CheckServerJob::instanceFound, this, [this] {
        const QPair<QString, QString> urls = Theme::instance()->oauthOverrideAuthUrl();
        if (!urls.first.isNull()) {
            OC_ASSERT(!urls.second.isNull());
            _authEndpoint = QUrl::fromUserInput(urls.first);
            _tokenEndpoint = QUrl::fromUserInput(urls.second);
            _wellKnownFinished = true;
            Q_EMIT fetchWellKnownFinished();
        } else {
            auto job = new SimpleNetworkJob(_account->sharedFromThis(), this);
            job->setAuthenticationJob(true);
            job->prepareRequest("GET", Utility::concatUrlPath(_account->url(), QStringLiteral("/.well-known/openid-configuration")));
            job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
            QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
                _wellKnownFinished = true;
                if (reply->error() != QNetworkReply::NoError) {
                    // Most likely the file does not exist, default to the normal endpoint
                    Q_EMIT fetchWellKnownFinished();
                    return;
                }
                const auto jsonData = reply->readAll();
                QJsonParseError jsonParseError;
                const QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();
                if (jsonParseError.error == QJsonParseError::NoError) {
                    _authEndpoint = QUrl::fromEncoded(json[QStringLiteral("authorization_endpoint")].toString().toUtf8());
                    _tokenEndpoint = QUrl::fromEncoded(json[QStringLiteral("token_endpoint")].toString().toUtf8());
                    _registrationEndpoint = QUrl::fromEncoded(json[QStringLiteral("registration_endpoint")].toString().toUtf8());
                } else if (jsonParseError.error == QJsonParseError::IllegalValue) {
                    qCDebug(lcOauth) << ".well-known did not return json, the server most probably does not support oidc";
                } else {
                    qCWarning(lcOauth) << "Json parse error in well-known: " << jsonParseError.errorString();
                }
                Q_EMIT fetchWellKnownFinished();
            });
            job->start();
        }
    });
    checkServer->start();
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

#include "oauth.moc"
