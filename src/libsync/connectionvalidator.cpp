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
    QString re;

    switch( stat ) {
    case Undefined:
        re = QLatin1String("Undefined");
        break;
    case Connected:
        re = QLatin1String("Connected");
        break;
    case NotConfigured:
        re = QLatin1String("NotConfigured");
        break;
    case ServerVersionMismatch:
        re = QLatin1String("Server Version Mismatch");
        break;
    case CredentialsWrong:
        re = QLatin1String("Credentials Wrong");
        break;
    case StatusNotFound:
        re = QLatin1String("Status not found");
        break;
    default:
        re = QLatin1String("status undeclared.");
    }
    return re;
}

bool ConnectionValidator::isNetworkError( Status status )
{
    return status == StatusNotFound;
}

void ConnectionValidator::checkConnection()
{
    if( !_account ) {
        _errors << tr("No ownCloud account configured");
        reportResult( NotConfigured );
        return;
    }

    if( _account->state() == Account::Connected ) {
        // When we're already connected, just make sure a minimal request
        // gets replied to.
        slotCheckAuthentication();
    } else {
        CheckServerJob *checkJob = new CheckServerJob(_account, this);
        checkJob->setIgnoreCredentialFailure(true);
        connect(checkJob, SIGNAL(instanceFound(QUrl,QVariantMap)), SLOT(slotStatusFound(QUrl,QVariantMap)));
        connect(checkJob, SIGNAL(networkError(QNetworkReply*)), SLOT(slotNoStatusFound(QNetworkReply*)));
        connect(checkJob, SIGNAL(timeout(QUrl)), SLOT(slotJobTimeout(QUrl)));
        checkJob->start();
    }
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
        QTimer::singleShot( 0, this, SLOT( slotCheckAuthentication() ));
    } else {
        connect( creds, SIGNAL(fetched()),
                 this, SLOT(slotCheckAuthentication()), Qt::UniqueConnection);
        creds->fetch(_account);
    }
}

// status.php could not be loaded.
void ConnectionValidator::slotNoStatusFound(QNetworkReply *reply)
{
    _account->setState(Account::Disconnected);

    _errors.append(tr("Unable to connect to %1").arg(_account->url().toString()));
    _errors.append( reply->errorString() );
    reportResult( StatusNotFound );
}

void ConnectionValidator::slotJobTimeout(const QUrl &url)
{
    _account->setState(Account::Disconnected);

    _errors.append(tr("Unable to connect to %1").arg(url.toString()));
    _errors.append(tr("timeout"));
    reportResult( StatusNotFound );
}


void ConnectionValidator::slotCheckAuthentication()
{
    AbstractCredentials *creds = _account->credentials();
    disconnect( creds, SIGNAL(fetched()),
                this, SLOT(slotCheckAuthentication()));

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
    Status stat = StatusNotFound;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ||
            reply->error() == QNetworkReply::OperationCanceledError ) { // returned if the user/pwd is wrong.
        qDebug() <<  reply->error() << reply->errorString();
        qDebug() << "******** Password is wrong!";
        _errors << tr("The provided credentials are not correct");
        stat = CredentialsWrong;
        if (_account->state() != Account::SignedOut) {
            _account->setState(Account::Disconnected);
        }

    } else if( reply->error() != QNetworkReply::NoError ) {
        _errors << reply->errorString();
    }

    reportResult( stat );
}

void ConnectionValidator::slotAuthSuccess()
{
    _account->setState(Account::Connected);
    _errors.clear();
    reportResult(Connected);
}

void ConnectionValidator::reportResult(Status status)
{
    emit connectionResult(status, _errors);
    deleteLater();
}

} // namespace OCC
