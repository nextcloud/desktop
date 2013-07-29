/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include "mirall/creds/httpcredentials.h"
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/utility.h"
#include "mirall/credentialstore.h"

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
    MirallConfigFile cfg;
    HttpCredentials* http_credentials = dynamic_cast< HttpCredentials* > (cfg.getCredentials());

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
        if( qPrompt.startsWith( QLatin1String("There are problems with the SSL certificate:"))) {
            // SSL is requested. If the program came here, the SSL check was done by mirall
            // It needs to be checked if the  chain is still equal to the one which
            // was verified by the user.
            QRegExp regexp("fingerprint: ([\\w\\d:]+)");
            bool certOk = false;

            int pos = 0;

            // This is the set of certificates which QNAM accepted, so we should accept
            // them as well
            QList<QSslCertificate> certs = ownCloudInfo::instance()->certificateChain();

            while (!certOk && (pos = regexp.indexIn(qPrompt, 1+pos)) != -1) {
                QString neon_fingerprint = regexp.cap(1);

                foreach( const QSslCertificate& c, certs ) {
                    QString verified_shasum = Utility::formatFingerprint(c.digest(QCryptographicHash::Sha1).toHex());
                    qDebug() << "SSL Fingerprint from neon: " << neon_fingerprint << " compared to verified: " << verified_shasum;
                    if( verified_shasum == neon_fingerprint ) {
                        certOk = true;
                        break;
                    }
                }
            }
            // certOk = false;     DEBUG setting, keep disabled!
            if( !certOk ) { // Problem!
                qstrcpy( buf, "no" );
                re = -1;
            } else {
                qstrcpy( buf, "yes" ); // Certificate is fine!
            }
        } else {
            qDebug() << "Unknown prompt: <" << prompt << ">";
            re = -1;
        }
    }
    return re;
}

} // ns

HttpCredentials::HttpCredentials()
    : _user(),
      _password(),
      _ready(false),
      _attempts()
{}

HttpCredentials::HttpCredentials(const QString& user, const QString& password)
    : _user(user),
      _password(password),
      _ready(true)
{}

void HttpCredentials::prepareSyncContext (CSYNC* ctx)
{
    // TODO: This should not be a part of this method, but we don't have
    // any way to get "session_key" module property from csync. Had we
    // have it, then we could remove this code and keep it in
    // csyncthread code (or folder code, git remembers).
    QList<QNetworkCookie> cookies(ownCloudInfo::instance()->getLastAuthCookies());
    QString cookiesAsString;

    // Stuff cookies inside csync, then we can avoid the intermediate HTTP 401 reply
    // when https://github.com/owncloud/core/pull/4042 is merged.
    foreach(QNetworkCookie c, cookies) {
        cookiesAsString += c.name();
        cookiesAsString += '=';
        cookiesAsString += c.value();
        cookiesAsString += "; ";
    }

    csync_set_module_property(ctx, "session_key", cookiesAsString.toLatin1().data());
    csync_set_auth_callback (ctx, getauth);
}

bool HttpCredentials::changed(AbstractCredentials* credentials) const
{
    HttpCredentials* other(dynamic_cast< HttpCredentials* >(credentials));

    if (!other || other->user() != this->user()) {
        return true;
    }

    return false;
}

QString HttpCredentials::authType() const
{
    return QString::fromLatin1("http");
}

QString HttpCredentials::user() const
{
    return _user;
}

QString HttpCredentials::password() const
{
    return _password;
}

QNetworkAccessManager* HttpCredentials::getQNAM() const
{
    QNetworkAccessManager* qnam = new QNetworkAccessManager;

    connect( qnam, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)),
             this, SLOT(slotAuthentication(QNetworkReply*,QAuthenticator*)));

    return qnam;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

void HttpCredentials::fetch()
{
    if (_ready) {
        Q_EMIT fetched();
    } else {
        // TODO: merge CredentialStore into HttpCredentials?
        CredentialStore* store(CredentialStore::instance());
        connect(store, SIGNAL(fetchCredentialsFinished(bool)),
                this, SLOT(slotCredentialsFetched(bool)));
        store->fetchCredentials();
    }
}

void HttpCredentials::persistForUrl(const QString& url)
{
    CredentialStore* store(CredentialStore::instance());
    store->setCredentials(url, _user, _password);
    store->saveCredentials();
}

void HttpCredentials::slotCredentialsFetched(bool ok)
{
    _ready = ok;
    if (_ready) {
        CredentialStore* store(CredentialStore::instance());
        _user = store->user();
        _password = store->password();
    }
    Q_EMIT fetched();
}

void HttpCredentials::slotAuthentication(QNetworkReply* reply, QAuthenticator* authenticator)
{
    if( !(authenticator && reply) ) return;

    qDebug() << "Authenticating request for " << reply->url();

    if (_attempts.contains(reply)) {
        ++_attempts[reply];
    } else {
        connect(reply, SIGNAL(finished()),
                this, SLOT(slotReplyFinished()));
        _attempts[reply] = 1;
    }
    // TODO: Replace it with something meaningful...
    //if( reply->url().toString().startsWith( webdavUrl( _connection ) ) ) {
    if (_attempts[reply] > 1) {
        qDebug() << "Too many attempts to authenticate. Stop request.";
        reply->close();
    } else {
        authenticator->setUser( _user );
        authenticator->setPassword( _password );
    }
    //} else {
    //    qDebug() << "WRN: attempt to authenticate to different url - closing.";
    // reply->close();
    //}
}

void HttpCredentials::slotReplyFinished()
{
    QNetworkReply* reply = qobject_cast< QNetworkReply* >(sender());

    disconnect(reply, SIGNAL(finished()),
               this, SLOT(slotReplyFinished()));
    _attempts.remove (reply);
}

} // ns Mirall
