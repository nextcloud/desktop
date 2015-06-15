/*
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
#include <QDebug>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QSettings>
#include <QInputDialog>

#include <keychain.h>

#include "account.h"
#include "accessmanager.h"
#include "utility.h"
#include "theme.h"
#include "syncengine.h"
#include "creds/credentialscommon.h"
#include "creds/httpcredentials.h"

using namespace QKeychain;

namespace OCC
{

int getauth(const char *prompt,
            char *buf,
            size_t len,
            int echo,
            int verify,
            void *userdata)
{
    int re = 0;

    // ### safe?  Not really.  If the wizard is run in the main thread, the caccount could change during the sync.
    SyncEngine* engine = reinterpret_cast<SyncEngine*>(userdata);
    HttpCredentials* http_credentials = qobject_cast<HttpCredentials*>(engine->account()->credentials());

    if (!http_credentials) {
      qDebug() << "Not a HTTP creds instance!";
      return -1;
    }

    QString qPrompt = QString::fromLatin1( prompt ).trimmed();
    QString user = http_credentials->user();
    QString pwd  = http_credentials->password();

    if( qPrompt == QLatin1String("Enter your username:") ) {
        // qDebug() << "OOO Username requested!";
        qstrncpy( buf, user.toUtf8().constData(), len );
    } else if( qPrompt == QLatin1String("Enter your password:") ) {
        // qDebug() << "OOO Password requested!";
        qstrncpy( buf, pwd.toUtf8().constData(), len );
    } else {
        if( http_credentials->sslIsTrusted() ) {
            qstrcpy( buf, "yes" ); // Certificate is fine!
        } else {
            re = handleNeonSSLProblems(prompt, buf, len, echo, verify, userdata);
        }
    }
    return re;
}

namespace
{
const char userC[] = "user";
const char certifPathC[] = "certificatePath";
const char certifPasswdC[] = "certificatePasswd";
const char authenticationFailedC[] = "owncloud-authentication-failed";
} // ns

HttpCredentials::HttpCredentials()
    : _user(),
      _password(),
      _certificatePath(),
      _certificatePasswd(),
      _ready(false),
      _fetchJobInProgress(false),
      _readPwdFromDeprecatedPlace(false)
{
}

HttpCredentials::HttpCredentials(const QString& user, const QString& password, const QString& certificatePath, const QString& certificatePasswd)
    : _user(user),
      _password(password),
      _certificatePath(certificatePath),
      _certificatePasswd(certificatePasswd),
      _ready(true),
      _fetchJobInProgress(false)
{
}

void HttpCredentials::syncContextPreInit (CSYNC* ctx)
{
    csync_set_auth_callback (ctx, getauth);
    // create a SSL client certificate configuration in CSYNC* ctx
    struct csync_client_certs_s clientCerts;
    clientCerts.certificatePath = strdup(_certificatePath.toStdString().c_str());
    clientCerts.certificatePasswd = strdup(_certificatePasswd.toStdString().c_str());
    csync_set_module_property(ctx, "SSLClientCerts", &clientCerts);
    free(clientCerts.certificatePath);
    free(clientCerts.certificatePasswd);
}

void HttpCredentials::syncContextPreStart (CSYNC* ctx)
{
    QList<QNetworkCookie> cookies(_account->lastAuthCookies());
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
}

bool HttpCredentials::changed(AbstractCredentials* credentials) const
{
    HttpCredentials* other(qobject_cast< HttpCredentials* >(credentials));

    if (!other) {
        return true;
    }

    if (!other || (other->user() != this->user())) {
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

QString HttpCredentials::certificatePath() const
{
    return _certificatePath;
}

QString HttpCredentials::certificatePasswd() const
{
    return _certificatePasswd;
}

void HttpCredentials::setAccount(Account* account)
{
    AbstractCredentials::setAccount(account);
    if (_user.isEmpty()) {
        fetchUser();
    }
}

QNetworkAccessManager* HttpCredentials::getQNAM() const
{
    AccessManager* qnam = new AccessManager;

    connect( qnam, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)),
             this, SLOT(slotAuthentication(QNetworkReply*,QAuthenticator*)));

    return qnam;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

QString HttpCredentials::fetchUser()
{
    _user = _account->credentialSetting(QLatin1String(userC)).toString();
    return _user;
}

void HttpCredentials::fetch()
{
    if (_fetchJobInProgress) {
        return;
    }

    // User must be fetched from config file
    fetchUser();
    _certificatePath = _account->credentialSetting(QLatin1String(certifPathC)).toString();
    _certificatePasswd = _account->credentialSetting(QLatin1String(certifPasswdC)).toString();

    auto settings = _account->settingsWithGroup(Theme::instance()->appName());
    const QString kck = keychainKey(_account->url().toString(), _user );

    QString key = QString::fromLatin1( "%1/data" ).arg( kck );
    if( settings && settings->contains(key) ) {
        // Clean the password from the config file if it is in there.
        // we do not want a security problem.
        settings->remove(key);
        key = QString::fromLatin1( "%1/type" ).arg( kck );
        settings->remove(key);
        settings->sync();
    }

    if (_ready) {
        Q_EMIT fetched();
    } else {
        ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
        settings->setParent(job); // make the job parent to make setting deleted properly
        job->setSettings(settings.release());

        job->setInsecureFallback(false);
        job->setKey(kck);
        connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotReadJobDone(QKeychain::Job*)));
        job->start();
        _fetchJobInProgress = true;
        _readPwdFromDeprecatedPlace = true;
    }
}
bool HttpCredentials::stillValid(QNetworkReply *reply)
{
    return (reply->error() != QNetworkReply::AuthenticationRequiredError);
}

void HttpCredentials::slotReadJobDone(QKeychain::Job *job)
{
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob*>(job);
    _password = readJob->textData();

    if( _user.isEmpty()) {
        qDebug() << "Strange: User is empty!";
    }

    QKeychain::Error error = job->error();

    if( !_password.isEmpty() && error == NoError ) {
        _fetchJobInProgress = false;

        // All cool, the keychain did not come back with error.
        // Still, the password can be empty which indicates a problem and
        // the password dialog has to be opened.
        _ready = true;
        emit fetched();
    } else {
        // we come here if the password is empty or any other keychain
        // error happend.
        // In all error conditions it should
        // ask the user for the password interactively now.
        if( _readPwdFromDeprecatedPlace ) {
            // there simply was not a password. Lets restart a read job without
            // a settings object as we did it in older client releases.
            ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());

            const QString kck = keychainKey(_account->url().toString(), _user);
            job->setKey(kck);

            connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotReadJobDone(QKeychain::Job*)));
            job->start();
            _readPwdFromDeprecatedPlace = false; // do  try that only once.
            _fetchJobInProgress = true;
            // Note: if this read job succeeds, the value from the old place is still
            // NOT persisted into the new account.
        } else {
            // interactive password dialog starts here
            bool ok;
            QString pwd = queryPassword(&ok);
            _fetchJobInProgress = false;
            if (ok) {
                _password = pwd;
                _ready = true;
                persist();
            } else {
                _password = QString::null;
                _ready = false;
            }
            emit fetched();
        }
    }
}

void HttpCredentials::invalidateToken()
{
    if (! _password.isEmpty()) {
        _previousPassword = _password;
    }
    _password = QString();
    _ready = false;

    // User must be fetched from config file to generate a valid key
    fetchUser();

    const QString kck = keychainKey(_account->url().toString(), _user);
    if( kck.isEmpty() ) {
        qDebug() << "InvalidateToken: User is empty, bailing out!";
        return;
    }

    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    auto settings = _account->settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings.release());
    job->setInsecureFallback(true);
    job->setKey(kck);
    job->start();

    // Also ensure the password is deleted from the deprecated place
    // otherwise we'd possibly read and use it again and again.
    DeletePasswordJob *job2 = new DeletePasswordJob(Theme::instance()->appName());
    // no job2->setSettings() call here, to make it use the deprecated location.
    job2->setInsecureFallback(true);
    job2->setKey(kck);
    job2->start();

    _account->clearCookieJar();
}

void HttpCredentials::persist()
{
    if (_user.isEmpty()) {
        // We never connected or fetched the user, there is nothing to save.
        return;
    }
    _account->setCredentialSetting(QLatin1String(userC), _user);
    _account->setCredentialSetting(QLatin1String(certifPathC), _certificatePath);
    _account->setCredentialSetting(QLatin1String(certifPasswdC), _certificatePasswd);
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    auto settings = _account->settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings.release());

    job->setInsecureFallback(false);
    connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotWriteJobDone(QKeychain::Job*)));
    job->setKey(keychainKey(_account->url().toString(), _user));
    job->setTextData(_password);
    job->start();
}

void HttpCredentials::slotWriteJobDone(QKeychain::Job *job)
{
    delete job->settings();
    switch (job->error()) {
    case NoError:
        break;
    default:
        qDebug() << "Error while writing password" << job->errorString();
    }
    WritePasswordJob *wjob = qobject_cast<WritePasswordJob*>(job);
    wjob->deleteLater();
}

void HttpCredentials::slotAuthentication(QNetworkReply* reply, QAuthenticator* authenticator)
{
    if (reply->property(authenticationFailedC).toBool()) {
        qDebug() << "Authentication failed for " << reply->url().toString();
        return;
    }

    // QNAM sends the user and password in latin-1,  but the server expects UTF-8.
    // So send mojibake on purpose
    authenticator->setUser(QString::fromLatin1(user().toUtf8()));
    authenticator->setPassword(QString::fromLatin1(password().toUtf8()));

    // Set a property so we don't send the same password twice
    reply->setProperty(authenticationFailedC, true);
}

} // namespace OCC
