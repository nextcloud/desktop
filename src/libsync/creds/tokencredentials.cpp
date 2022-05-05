/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (c) by Markus Goetz <guruz@owncloud.com>
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

#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkReply>
#include <QSettings>
#include <QNetworkCookieJar>

#include "account.h"
#include "accessmanager.h"
#include "common/utility.h"
#include "theme.h"
#include "creds/credentialscommon.h"
#include "creds/tokencredentials.h"


namespace OCC {

Q_LOGGING_CATEGORY(lcTokenCredentials, "sync.credentials.token", QtInfoMsg)

namespace {

    const char authenticationFailedC[] = "owncloud-authentication-failed";

} // ns

class TokenCredentialsAccessManager : public AccessManager
{
public:
    friend class TokenCredentials;
    TokenCredentialsAccessManager(const TokenCredentials *cred, QObject *parent = 0)
        : AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
    {
        if (_cred->user().isEmpty() || _cred->password().isEmpty()) {
            qCWarning(lcTokenCredentials) << "Empty user/password provided!";
        }

        QNetworkRequest req(request);

        QByteArray credHash = QByteArray(_cred->user().toUtf8() + ":" + _cred->password().toUtf8()).toBase64();
        req.setRawHeader(QByteArray("Authorization"), QByteArray("Basic ") + credHash);

        // A pre-authenticated cookie
        QByteArray token = _cred->_token.toUtf8();
        if (token.length() > 0) {
            setRawCookie(token, request.url());
        }

        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    const TokenCredentials *_cred;
};

TokenCredentials::TokenCredentials()
    : _user()
    , _password()
    , _ready(false)
{
}

TokenCredentials::TokenCredentials(const QString &user, const QString &password, const QString &token)
    : _user(user)
    , _password(password)
    , _token(token)
    , _ready(true)
{
}

QString TokenCredentials::authType() const
{
    return QString::fromLatin1("token");
}

QString TokenCredentials::user() const
{
    return _user;
}

QString TokenCredentials::password() const
{
    return _password;
}

AccessManager *TokenCredentials::createAM() const
{
    AccessManager *am = new TokenCredentialsAccessManager(this);

    connect(am, &AccessManager::authenticationRequired,
        this, &TokenCredentials::slotAuthentication);

    return am;
}

bool TokenCredentials::ready() const
{
    return _ready;
}

void TokenCredentials::fetchFromKeychain()
{
    _wasFetched = true;
    Q_EMIT fetched();
}

void TokenCredentials::askFromUser()
{
    emit asked();
}


bool TokenCredentials::stillValid(QNetworkReply *reply)
{
    return ((reply->error() != QNetworkReply::AuthenticationRequiredError)
        // returned if user/password or token are incorrect
        && (reply->error() != QNetworkReply::OperationCanceledError
               || !reply->property(authenticationFailedC).toBool()));
}

void TokenCredentials::invalidateToken()
{
    qCInfo(lcTokenCredentials) << "Invalidating token";
    _ready = false;
    _account->clearCookieJar();
    _token = QString();
    _user = QString();
    _password = QString();
}

void TokenCredentials::forgetSensitiveData()
{
    invalidateToken();
}

void TokenCredentials::persist()
{
}


void TokenCredentials::slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    Q_UNUSED(authenticator)
    // we cannot use QAuthenticator, because it sends username and passwords with latin1
    // instead of utf8 encoding. Instead, we send it manually. Thus, if we reach this signal,
    // those credentials were invalid and we terminate.
    qCWarning(lcTokenCredentials) << "Stop request: Authentication failed for " << reply->url().toString();
    reply->setProperty(authenticationFailedC, true);
    reply->close();
}

} // namespace OCC
