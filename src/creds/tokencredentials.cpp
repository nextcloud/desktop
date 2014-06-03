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

#include "mirall/account.h"
#include "mirall/mirallaccessmanager.h"
#include "mirall/utility.h"
#include "mirall/theme.h"
#include "creds/credentialscommon.h"
#include "creds/tokencredentials.h"


namespace Mirall
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

class TokenCredentialsAccessManager : public MirallAccessManager {
public:
    friend class TokenCredentials;
    TokenCredentialsAccessManager(const TokenCredentials *cred, QObject* parent = 0)
        : MirallAccessManager(parent), _cred(cred) {}
protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) {
        QByteArray credHash = QByteArray(_cred->user().toUtf8()+":"+_cred->password().toUtf8()).toBase64();
        QNetworkRequest req(request);
        req.setRawHeader(QByteArray("Authorization"), QByteArray("Basic ") + credHash);
        //qDebug() << "Request for " << req.url() << "with authorization" << QByteArray::fromBase64(credHash);

        // Append token cookie
        QList<QNetworkCookie> cookies = request.header(QNetworkRequest::CookieHeader).value<QList<QNetworkCookie> >();
        cookies.append(QNetworkCookie::parseCookies(_cred->_token.toUtf8()));
        req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(cookies));

        return MirallAccessManager::createRequest(op, req, outgoingData);
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
    MirallAccessManager* qnam = new TokenCredentialsAccessManager(this);

    connect( qnam, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)),
             this, SLOT(slotAuthentication(QNetworkReply*,QAuthenticator*)));

    return qnam;
}

bool TokenCredentials::ready() const
{
    return _ready;
}

QString TokenCredentials::fetchUser(Account* account)
{
    _user = account->credentialSetting(QLatin1String(userC)).toString();
    return _user;
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
            // returned if user or password is incorrect
            && (reply->error() != QNetworkReply::OperationCanceledError
                || !reply->property(authenticationFailedC).toBool()));
}

QString TokenCredentials::queryPassword(bool *ok)
{
    return QString();
}

void TokenCredentials::invalidateToken(Account *account)
{
    _password = QString();
    _ready = false;

    // User must be fetched from config file to generate a valid key
    fetchUser(account);

    const QString kck = keychainKey(account->url().toString(), _user);
    if( kck.isEmpty() ) {
        qDebug() << "InvalidateToken: User is empty, bailing out!";
        return;
    }

    account->clearCookieJar();
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

} // ns Mirall
