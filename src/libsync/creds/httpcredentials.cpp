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

namespace
{
const char userC[] = "user";
const char certifPathC[] = "certificatePath";
const char certifPasswdC[] = "certificatePasswd";
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
    : _ready(false)
{
}

HttpCredentials::HttpCredentials(const QString& user, const QString& password, const QString& certificatePath, const QString& certificatePasswd)
    : _user(user),
      _password(password),
      _ready(true),
      _certificatePath(certificatePath),
      _certificatePasswd(certificatePasswd)
{
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
    AccessManager* qnam = new HttpCredentialsAccessManager(this);

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

void HttpCredentials::fetchFromKeychain()
{
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

    if( _user.isEmpty()) {
        qDebug() << "Strange: User is empty!";
    }

    QKeychain::Error error = job->error();

    if( !_password.isEmpty() && error == NoError ) {

        // All cool, the keychain did not come back with error.
        // Still, the password can be empty which indicates a problem and
        // the password dialog has to be opened.
        _ready = true;
        emit fetched();
    } else {
        // we come here if the password is empty or any other keychain
        // error happend.

        _fetchErrorString = job->error() != EntryNotFound ? job->errorString() : QString();

        _password = QString();
        _ready = false;
        emit fetched();
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

    // clear the session cookie.
    _account->clearCookieJar();

    // let QNAM forget about the password
    // This needs to be done later in the event loop because we might be called (directly or
    // indirectly) from QNetworkAccessManagerPrivate::authenticationRequired, which itself
    // is a called from a BlockingQueuedConnection from the Qt HTTP thread. And clearing the
    // cache needs to synchronize again with the HTTP thread.
    QTimer::singleShot(0, this, SLOT(clearQNAMCache()));
}

void HttpCredentials::clearQNAMCache()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    _account->networkAccessManager()->clearAccessCache();
#else
    _account->resetNetworkAccessManager();
#endif
}

void HttpCredentials::forgetSensitiveData()
{
    invalidateToken();
    _previousPassword.clear();
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
    Q_UNUSED(authenticator)
    // Because of issue #4326, we need to set the login and password manually at every requests
    // Thus, if we reach this signal, those credentials were invalid and we terminate.
    qDebug() << "Stop request: Authentication failed for " << reply->url().toString();
    reply->setProperty(authenticationFailedC, true);
    reply->close();
}

} // namespace OCC
