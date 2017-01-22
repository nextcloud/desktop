/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QtCore>
#include <QNetworkReply>
#include <QNetworkProxyFactory>
#include <QPixmap>

#include "connectionvalidator.h"
#include "account.h"
#include "networkjobs.h"
#include "clientproxy.h"
#include <creds/abstractcredentials.h>

namespace OCC {

// Make sure the timeout for this job is less than how often we get called
// This makes sure we get tried often enough without "ConnectionValidator already running"
static qint64 timeoutToUseMsec = qMax(1000, ConnectionValidator::DefaultCallingIntervalMsec - 5*1000);

ConnectionValidator::ConnectionValidator(AccountPtr account, QObject *parent)
    : QObject(parent),
      _account(account),
      _isCheckingServerAndAuth(false)
{
}

QString ConnectionValidator::statusString( Status stat )
{
    switch( stat ) {
    case Undefined:
        return QLatin1String("Undefined");
    case Connected:
        return QLatin1String("Connected");
    case NotConfigured:
        return QLatin1String("NotConfigured");
    case ServerVersionMismatch:
        return QLatin1String("Server Version Mismatch");
    case CredentialsMissingOrWrong:
        return QLatin1String("Credentials Wrong");
    case StatusNotFound:
        return QLatin1String("Status not found");
    case UserCanceledCredentials:
        return QLatin1String("User canceled credentials");
    case ServiceUnavailable:
        return QLatin1String("Service unavailable");
    case Timeout:
        return QLatin1String("Timeout");
    }
    return QLatin1String("status undeclared.");
}

void ConnectionValidator::checkServerAndAuth()
{
    if( !_account ) {
        _errors << tr("No ownCloud account configured");
        reportResult( NotConfigured );
        return;
    }
    qDebug() << "Checking server and authentication";

    _isCheckingServerAndAuth = true;

    // Lookup system proxy in a thread https://github.com/owncloud/client/issues/2993
    if (ClientProxy::isUsingSystemDefault()) {
        qDebug() << "Trying to look up system proxy";
        ClientProxy::lookupSystemProxyAsync(_account->url(),
                                            this, SLOT(systemProxyLookupDone(QNetworkProxy)));
    } else {
        // We want to reset the QNAM proxy so that the global proxy settings are used (via ClientProxy settings)
        _account->networkAccessManager()->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        // use a queued invocation so we're as asynchronous as with the other code path
        QMetaObject::invokeMethod(this, "slotCheckServerAndAuth", Qt::QueuedConnection);
    }
}

void ConnectionValidator::systemProxyLookupDone(const QNetworkProxy &proxy) {
    if (!_account) {
        qDebug() << "Bailing out, Account had been deleted";
        return;
    }

    if (proxy.type() != QNetworkProxy::NoProxy) {
        qDebug() << "Setting QNAM proxy to be system proxy" << printQNetworkProxy(proxy);
    } else {
        qDebug() << "No system proxy set by OS";
    }
    _account->networkAccessManager()->setProxy(proxy);

    slotCheckServerAndAuth();
}

// The actual check
void ConnectionValidator::slotCheckServerAndAuth()
{
    CheckServerJob *checkJob = new CheckServerJob(_account, this);
    checkJob->setTimeout(timeoutToUseMsec);
    checkJob->setIgnoreCredentialFailure(true);
    connect(checkJob, SIGNAL(instanceFound(QUrl,QVariantMap)), SLOT(slotStatusFound(QUrl,QVariantMap)));
    connect(checkJob, SIGNAL(instanceNotFound(QNetworkReply*)), SLOT(slotNoStatusFound(QNetworkReply*)));
    connect(checkJob, SIGNAL(timeout(QUrl)), SLOT(slotJobTimeout(QUrl)));
    checkJob->start();
}

void ConnectionValidator::slotStatusFound(const QUrl&url, const QVariantMap &info)
{
    // status.php was found.
    qDebug() << "** Application: ownCloud found: "
             << url << " with version "
             << CheckServerJob::versionString(info)
             << "(" << CheckServerJob::version(info) << ")";

    QString version = CheckServerJob::version(info);
    _account->setServerVersion(version);

    // We cannot deal with servers < 5.0.0
    if (version.contains('.') && version.split('.')[0].toInt() < 5) {
        _errors.append( tr("The configured server for this client is too old") );
        _errors.append( tr("Please update to the latest server and restart the client.") );
        reportResult( ServerVersionMismatch );
        return;
    }

    // We attempt to work with servers >= 5.0.0 but warn users.
    // Check usages of Account::serverVersionUnsupported() for details.

    // now check the authentication
    if (_account->credentials()->ready())
        QTimer::singleShot( 0, this, SLOT( checkAuthentication() ));
    else
        reportResult( CredentialsMissingOrWrong );
}

// status.php could not be loaded (network or server issue!).
void ConnectionValidator::slotNoStatusFound(QNetworkReply *reply)
{
    qDebug() << Q_FUNC_INFO << reply->error() << reply->errorString() << reply->peek(1024);
    if (reply && !_account->credentials()->ready()) {
        // This could be needed for SSL client certificates
        // We need to load them from keychain and try
        reportResult( CredentialsMissingOrWrong );
    } else
    if( reply && ! _account->credentials()->stillValid(reply)) {
        _errors.append(tr("Authentication error: Either username or password are wrong."));
    }  else {
        //_errors.append(tr("Unable to connect to %1").arg(_account->url().toString()));
        _errors.append( reply->errorString() );
    }
    reportResult( StatusNotFound );
}

void ConnectionValidator::slotJobTimeout(const QUrl &url)
{
    Q_UNUSED(url);
    //_errors.append(tr("Unable to connect to %1").arg(url.toString()));
    _errors.append(tr("timeout"));
    reportResult( Timeout );
}


void ConnectionValidator::checkAuthentication()
{
    AbstractCredentials *creds = _account->credentials();

    if (!creds->ready()) { // The user canceled
        reportResult(UserCanceledCredentials);
    }

    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
    qDebug() << "# Check whether authenticated propfind works.";
    PropfindJob *job = new PropfindJob(_account, "/", this);
    job->setTimeout(timeoutToUseMsec);
    job->setProperties(QList<QByteArray>() << "getlastmodified");
    connect(job, SIGNAL(result(QVariantMap)), SLOT(slotAuthSuccess()));
    connect(job, SIGNAL(finishedWithError(QNetworkReply*)), SLOT(slotAuthFailed(QNetworkReply*)));
    job->start();
}

void ConnectionValidator::slotAuthFailed(QNetworkReply *reply)
{
    Status stat = Timeout;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ||
             !_account->credentials()->stillValid(reply)) {
        qDebug() <<  reply->error() << reply->errorString();
        qDebug() << "******** Password is wrong!";
        _errors << tr("The provided credentials are not correct");
        stat = CredentialsMissingOrWrong;

    } else if( reply->error() != QNetworkReply::NoError ) {
        _errors << errorMessage(reply->errorString(), reply->readAll());

        const int httpStatus =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if ( httpStatus == 503 ) {
            _errors.clear();
            stat = ServiceUnavailable;
        }
    }

    reportResult( stat );
}

void ConnectionValidator::slotAuthSuccess()
{
    _errors.clear();
    if (!_isCheckingServerAndAuth) {
        reportResult(Connected);
        return;
    }
    checkServerCapabilities();
}

void ConnectionValidator::checkServerCapabilities()
{
    JsonApiJob *job = new JsonApiJob(_account, QLatin1String("ocs/v1.php/cloud/capabilities"), this);
    job->setTimeout(timeoutToUseMsec);
    QObject::connect(job, SIGNAL(jsonReceived(QVariantMap, int)), this, SLOT(slotCapabilitiesRecieved(QVariantMap)));
    job->start();
}

void ConnectionValidator::slotCapabilitiesRecieved(const QVariantMap &json)
{
    auto caps = json.value("ocs").toMap().value("data").toMap().value("capabilities");
    qDebug() << "Server capabilities" << caps;
    _account->setCapabilities(caps.toMap());
    fetchUser();
}

void ConnectionValidator::fetchUser()
{

    JsonApiJob *job = new JsonApiJob(_account, QLatin1String("ocs/v1.php/cloud/user"), this);
    job->setTimeout(timeoutToUseMsec);
    QObject::connect(job, SIGNAL(jsonReceived(QVariantMap, int)), this, SLOT(slotUserFetched(QVariantMap)));
    job->start();
}

void ConnectionValidator::slotUserFetched(const QVariantMap &json)
{
    QString user = json.value("ocs").toMap().value("data").toMap().value("id").toString();
    if (!user.isEmpty()) {
        _account->setDavUser(user);

        AvatarJob *job = new AvatarJob(_account, this);
        QObject::connect(job, SIGNAL(avatarPixmap(QPixmap)), this, SLOT(slotAvatarPixmap(QPixmap)));

        job->start();
    }
}

void ConnectionValidator::slotAvatarPixmap(const QPixmap& pixmap)
{
    _account->setAvatar(pixmap);
    reportResult(Connected);
}

void ConnectionValidator::reportResult(Status status)
{
    emit connectionResult(status, _errors);
    deleteLater();
}

} // namespace OCC
