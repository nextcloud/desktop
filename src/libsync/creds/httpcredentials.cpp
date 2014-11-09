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
#include <QSettings>
#include <QInputDialog>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <qt5keychain/keychain.h>
#else
#include <qtkeychain/keychain.h>
#endif

#include "account.h"
#include "accessmanager.h"
#include "utility.h"
#include "theme.h"
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
    // Ideally, http_credentials could be use userdata,   but userdata is the SyncEngine.
    HttpCredentials* http_credentials = qobject_cast<HttpCredentials*>(AccountManager::instance()->account()->credentials());

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
const char authenticationFailedC[] = "owncloud-authentication-failed";
} // ns

class HttpCredentialsAccessManager : public AccessManager {
public:
    HttpCredentialsAccessManager(const HttpCredentials *cred, QObject* parent = 0)
        : AccessManager(parent), _cred(cred) {}
protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) Q_DECL_OVERRIDE {
        QByteArray credHash = QByteArray(_cred->user().toUtf8()+":"+_cred->password().toUtf8()).toBase64();
        QNetworkRequest req(request);
        req.setRawHeader(QByteArray("Authorization"), QByteArray("Basic ") + credHash);
        //qDebug() << "Request for " << req.url() << "with authorization" << QByteArray::fromBase64(credHash);
        return AccessManager::createRequest(op, req, outgoingData);
    }
private:
    const HttpCredentials *_cred;
};

HttpCredentials::HttpCredentials()
    : _user(),
      _password(),
      _ready(false),
      _fetchJobInProgress(false),
      _readPwdFromDeprecatedPlace(false)
{
}

HttpCredentials::HttpCredentials(const QString& user, const QString& password)
    : _user(user),
      _password(password),
      _ready(true),
      _fetchJobInProgress(false)
{
}

void HttpCredentials::syncContextPreInit (CSYNC* ctx)
{
    csync_set_auth_callback (ctx, getauth);
}

void HttpCredentials::syncContextPreStart (CSYNC* ctx)
{
    // TODO: This should not be a part of this method, but we don't have
    // any way to get "session_key" module property from csync. Had we
    // have it, then we could remove this code and keep it in
    // csyncthread code (or folder code, git remembers).
    QList<QNetworkCookie> cookies(AccountManager::instance()->account()->lastAuthCookies());
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

QNetworkAccessManager* HttpCredentials::getQNAM() const
{
    AccessManager* qnam = new HttpCredentialsAccessManager(this);

    connect( qnam, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)),
             this, SLOT(slotAuthentication(QNetworkReply*,QAuthenticator*)));

    return qnam;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

QString HttpCredentials::fetchUser(Account* account)
{
    _user = account->credentialSetting(QLatin1String(userC)).toString();
    return _user;
}

void HttpCredentials::fetch(Account *account)
{
    if( !account ) {
        return;
    }

    if (_fetchJobInProgress) {
        return;
    }

    // User must be fetched from config file
    fetchUser(account);

    QSettings *settings = account->settingsWithGroup(Theme::instance()->appName());
    const QString kck = keychainKey(account->url().toString(), _user );

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
        settings->deleteLater();
        Q_EMIT fetched();
    } else {
        ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
        settings->setParent(job); // make the job parent to make setting deleted properly
        job->setSettings(settings);

        job->setInsecureFallback(false);
        job->setKey(kck);
        connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotReadJobDone(QKeychain::Job*)));
        job->setProperty("account", QVariant::fromValue(account));
        job->start();
        _fetchJobInProgress = true;
        _readPwdFromDeprecatedPlace = true;
    }
}
bool HttpCredentials::stillValid(QNetworkReply *reply)
{
    return ((reply->error() != QNetworkReply::AuthenticationRequiredError)
            // returned if user or password is incorrect
            && (reply->error() != QNetworkReply::OperationCanceledError
                || !reply->property(authenticationFailedC).toBool()));
}

void HttpCredentials::slotReadJobDone(QKeychain::Job *job)
{
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob*>(job);
    _password = readJob->textData();
    Account *account = qvariant_cast<Account*>(readJob->property("account"));

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

            const QString kck = keychainKey(account->url().toString(), _user);
            job->setKey(kck);

            connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotReadJobDone(QKeychain::Job*)));
            job->setProperty("account", QVariant::fromValue(account));
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
                persist(account);
            } else {
                _password = QString::null;
                _ready = false;
            }
            emit fetched();
        }
    }
}

void HttpCredentials::invalidateToken(Account *account)
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

    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    QSettings *settings = account->settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings);
    job->setInsecureFallback(true);
    job->setKey(kck);
    job->start();

    account->clearCookieJar();
}

void HttpCredentials::persist(Account *account)
{
    if (_user.isEmpty()) {
        // We never connected or fetched the user, there is nothing to save.
        return;
    }
    account->setCredentialSetting(QLatin1String(userC), _user);
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    QSettings *settings = account->settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings);

    job->setInsecureFallback(false);
    connect(job, SIGNAL(finished(QKeychain::Job*)), SLOT(slotWriteJobDone(QKeychain::Job*)));
    job->setKey(keychainKey(account->url().toString(), _user));
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
    Q_UNUSED(authenticator)
    // we cannot use QAuthenticator, because it sends username and passwords with latin1
    // instead of utf8 encoding. Instead, we send it manually. Thus, if we reach this signal,
    // those credentials were invalid and we terminate.
    qDebug() << "Stop request: Authentication failed for " << reply->url().toString();
    reply->setProperty(authenticationFailedC, true);
    reply->close();
}

QString HttpCredentialsGui::queryPassword(bool *ok)
{
    if (ok) {
        QString str = QInputDialog::getText(0, tr("Enter Password"),
                                     tr("Please enter %1 password for user '%2':")
                                     .arg(Theme::instance()->appNameGUI(), _user),
                                     QLineEdit::Password, QString(), ok);
        return str;
    } else {
        return QString();
    }
}

} // namespace OCC
