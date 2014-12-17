/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QtCore>
#include <QNetworkReply>

#include "connectionvalidator.h"
#include "theme.h"
#include "account.h"
#include "networkjobs.h"
#include <creds/abstractcredentials.h>

namespace OCC {

ConnectionValidator::ConnectionValidator(Account *account, QObject *parent)
    : QObject(parent),
      _account(account)
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
    case CredentialsWrong:
        return QLatin1String("Credentials Wrong");
    case StatusNotFound:
        return QLatin1String("Status not found");
    case Timeout:
        return QLatin1String("Timeout");
    }
    return QLatin1String("status undeclared.");
}

bool ConnectionValidator::isNetworkError( Status status )
{
    return status == Timeout;
}

void ConnectionValidator::checkServerAndAuth()
{
    if( !_account ) {
        _errors << tr("No ownCloud account configured");
        reportResult( NotConfigured );
        return;
    }

    CheckServerJob *checkJob = new CheckServerJob(_account, this);
    checkJob->setIgnoreCredentialFailure(true);
    connect(checkJob, SIGNAL(instanceFound(QUrl,QVariantMap)), SLOT(slotStatusFound(QUrl,QVariantMap)));
    connect(checkJob, SIGNAL(networkError(QNetworkReply*)), SLOT(slotNoStatusFound(QNetworkReply*)));
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

    if( CheckServerJob::version(info).startsWith("4.0") ) {
        _errors.append( tr("The configured server for this client is too old") );
        _errors.append( tr("Please update to the latest server and restart the client.") );
        reportResult( ServerVersionMismatch );
        return;
    }

    // now check the authentication
    AbstractCredentials *creds = _account->credentials();
    if (creds->ready()) {
        QTimer::singleShot( 0, this, SLOT( checkAuthentication() ));
    } else {
        connect( creds, SIGNAL(fetched()),
                 this, SLOT(checkAuthentication()), Qt::UniqueConnection);
        creds->fetch(_account);
    }
}

// status.php could not be loaded.
void ConnectionValidator::slotNoStatusFound(QNetworkReply *reply)
{
    _errors.append(tr("Unable to connect to %1").arg(_account->url().toString()));
    _errors.append( reply->errorString() );
    reportResult( StatusNotFound );
}

void ConnectionValidator::slotJobTimeout(const QUrl &url)
{
    _errors.append(tr("Unable to connect to %1").arg(url.toString()));
    _errors.append(tr("timeout"));
    reportResult( Timeout );
}


void ConnectionValidator::checkAuthentication()
{
    AbstractCredentials *creds = _account->credentials();
    disconnect( creds, SIGNAL(fetched()),
                this, SLOT(checkAuthentication()));

    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
    qDebug() << "# Check whether authenticated propfind works.";
    PropfindJob *job = new PropfindJob(_account, "/", this);
    job->setProperties(QList<QByteArray>() << "getlastmodified");
    connect(job, SIGNAL(result(QVariantMap)), SLOT(slotAuthSuccess()));
    connect(job, SIGNAL(networkError(QNetworkReply*)), SLOT(slotAuthFailed(QNetworkReply*)));
    job->start();
}

void ConnectionValidator::slotAuthFailed(QNetworkReply *reply)
{
    Status stat = Timeout;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ||
            reply->error() == QNetworkReply::OperationCanceledError ) { // returned if the user/pwd is wrong.
        qDebug() <<  reply->error() << reply->errorString();
        qDebug() << "******** Password is wrong!";
        _errors << tr("The provided credentials are not correct");
        stat = CredentialsWrong;

    } else if( reply->error() != QNetworkReply::NoError ) {
        _errors << reply->errorString();
    }

    reportResult( stat );
}

void ConnectionValidator::slotAuthSuccess()
{
    _errors.clear();
    reportResult(Connected);
}

void ConnectionValidator::reportResult(Status status)
{
    emit connectionResult(status, _errors);
    deleteLater();
}

} // namespace OCC
