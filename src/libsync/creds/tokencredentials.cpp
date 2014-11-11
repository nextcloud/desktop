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

#include <QMutex>
#include <QDebug>
#include <QNetworkReply>
#include <QSettings>
#include <QNetworkCookieJar>

#include "account.h"
#include "accessmanager.h"
#include "utility.h"
#include "theme.h"
#include "creds/credentialscommon.h"
#include "creds/tokencredentials.h"


namespace OCC
{

namespace
{

int getauth(const char *prompt,
            char *buf,
            size_t len,
            int echo,
            int verify,
            void *userdata)
{
    int re = 0;
    QMutex mutex;
    // ### safe?
    TokenCredentials* http_credentials = qobject_cast<TokenCredentials*>(AccountManager::instance()->account()->credentials());

    if (!http_credentials) {
      qDebug() << "Not a HTTP creds instance!";
      return -1;
    }

    QString qPrompt = QString::fromLatin1( prompt ).trimmed();
    QString user = http_credentials->user();
    QString pwd  = http_credentials->password();

    if( qPrompt == QLatin1String("Enter your username:") ) {
        // qDebug() << "OOO Username requested!";
        QMutexLocker locker( &mutex );
        qstrncpy( buf, user.toUtf8().constData(), len );
    } else if( qPrompt == QLatin1String("Enter your password:") ) {
        QMutexLocker locker( &mutex );
        // qDebug() << "OOO Password requested!";
        qstrncpy( buf, pwd.toUtf8().constData(), len );
    } else {
        re = handleNeonSSLProblems(prompt, buf, len, echo, verify, userdata);
    }
    return re;
}

const char userC[] = "user";
const char authenticationFailedC[] = "owncloud-authentication-failed";

} // ns

class TokenCredentialsAccessManager : public AccessManager {
public:
    friend class TokenCredentials;
    TokenCredentialsAccessManager(const TokenCredentials *cred, QObject* parent = 0)
        : AccessManager(parent), _cred(cred) {}
protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) {
        if (_cred->user().isEmpty() || _cred->password().isEmpty() || _cred->_token.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "Empty user/password/token provided!";
        }

        QNetworkRequest req(request);

        QByteArray credHash = QByteArray(_cred->user().toUtf8()+":"+_cred->password().toUtf8()).toBase64();
        req.setRawHeader(QByteArray("Authorization"), QByteArray("Basic ") + credHash);

        // A pre-authenticated cookie
        QByteArray token = _cred->_token.toUtf8();
        setRawCookie(token, request.url());

        return AccessManager::createRequest(op, req, outgoingData);
    }
private:
    const TokenCredentials *_cred;
};

TokenCredentials::TokenCredentials()
    : _user(),
      _password(),
      _ready(false)
{
}

TokenCredentials::TokenCredentials(const QString& user, const QString& password, const QString &token)
    : _user(user),
      _password(password),
      _token(token),
      _ready(true)
{
}

void TokenCredentials::syncContextPreInit (CSYNC* ctx)
{
    csync_set_auth_callback (ctx, getauth);
}

void TokenCredentials::syncContextPreStart (CSYNC* ctx)
{
    csync_set_module_property(ctx, "session_key", _token.toUtf8().data());
}

bool TokenCredentials::changed(AbstractCredentials* credentials) const
{
    TokenCredentials* other(dynamic_cast< TokenCredentials* >(credentials));

    if (!other || (other->user() != this->user())) {
        return true;
    }

    return false;
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

QNetworkAccessManager* TokenCredentials::getQNAM() const
{
    AccessManager* qnam = new TokenCredentialsAccessManager(this);

    connect( qnam, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)),
             this, SLOT(slotAuthentication(QNetworkReply*,QAuthenticator*)));

    return qnam;
}

bool TokenCredentials::ready() const
{
    return _ready;
}

void TokenCredentials::fetch(Account *account)
{
    if( !account ) {
        return;
    }
    Q_EMIT fetched();
}

bool TokenCredentials::stillValid(QNetworkReply *reply)
{
    return ((reply->error() != QNetworkReply::AuthenticationRequiredError)
            // returned if user/password or token are incorrect
            && (reply->error() != QNetworkReply::OperationCanceledError
                || !reply->property(authenticationFailedC).toBool()));
}

QString TokenCredentials::queryPassword(bool *ok)
{
    return QString();
}

void TokenCredentials::invalidateToken(Account *account)
{
    qDebug() << Q_FUNC_INFO;
    _ready = false;
    account->clearCookieJar();
    _token = QString();
    _user = QString();
    _password = QString();
}

void TokenCredentials::persist(Account *account)
{
}


void TokenCredentials::slotAuthentication(QNetworkReply* reply, QAuthenticator* authenticator)
{
    Q_UNUSED(authenticator)
    // we cannot use QAuthenticator, because it sends username and passwords with latin1
    // instead of utf8 encoding. Instead, we send it manually. Thus, if we reach this signal,
    // those credentials were invalid and we terminate.
    qDebug() << "Stop request: Authentication failed for " << reply->url().toString();
    reply->setProperty(authenticationFailedC, true);
    reply->close();
}

} // namespace OCC
