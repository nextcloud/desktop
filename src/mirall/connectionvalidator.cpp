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

#include "mirall/connectionvalidator.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"
#include "mirall/account.h"
#include "mirall/networkjobs.h"

namespace Mirall {

ConnectionValidator::ConnectionValidator(Account *account, QObject *parent)
    : QObject(parent),
      _account(account),
      _networkError(QNetworkReply::NoError)
{
}

QStringList ConnectionValidator::errors() const
{
    return _errors;
}

bool ConnectionValidator::networkError() const
{
    return _networkError;
}

QString ConnectionValidator::statusString( Status stat ) const
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
    case CredentialsTooManyAttempts:
        re = QLatin1String("Credentials Too Many Attempts");
        break;
     case CredentialError:
        re = QLatin1String("CredentialError");
        break;
    case CredentialsUserCanceled:
        re = QLatin1String("Credential User Canceled");
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


void ConnectionValidator::checkConnection()
{
    if( _account ) {
        CheckServerJob *checkJob = new CheckServerJob(_account, false, this);
        checkJob->setIgnoreCredentialFailure(true);
        connect(checkJob, SIGNAL(instanceFound(QUrl,QVariantMap)), SLOT(slotStatusFound(QUrl,QVariantMap)));
        connect(checkJob, SIGNAL(networkError(QNetworkReply*)), SLOT(slotNoStatusFound(QNetworkReply*)));
        checkJob->start();
    } else {
        _errors << tr("No ownCloud account configured");
        emit connectionResult( NotConfigured );
    }
}

void ConnectionValidator::slotStatusFound(const QUrl&url, const QVariantMap &info)
{
    // status.php was found.
    qDebug() << "** Application: ownCloud found: "
             << url << " with version "
             << CheckServerJob::versionString(info)
             << "(" << CheckServerJob::version(info) << ")";
    // now check the authentication

    if( CheckServerJob::version(info).startsWith("4.0") ) {
        _errors.append( tr("The configured server for this client is too old") );
        _errors.append( tr("Please update to the latest server and restart the client.") );
        emit connectionResult( ServerVersionMismatch );
        return;
    }

    QTimer::singleShot( 0, this, SLOT( slotCheckAuthentication() ));
}

// status.php could not be loaded.
void ConnectionValidator::slotNoStatusFound(QNetworkReply *reply)
{
    _account->setState(Account::Disconnected);

    // ### TODO
    _errors.append(tr("Unable to connect to %1").arg(_account->url().toString()));
    _errors.append( reply->errorString() );
    _networkError = (reply->error() != QNetworkReply::NoError);
    emit connectionResult( StatusNotFound );

}

void ConnectionValidator::slotCheckAuthentication()
{
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
    PropfindJob *job = new PropfindJob(_account, "/", this);
    job->setIgnoreCredentialFailure(true);
    job->setProperties(QList<QByteArray>() << "getlastmodified");
    connect(job, SIGNAL(result(QVariantMap)), SLOT(slotAuthSuccess()));
    connect(job, SIGNAL(networkError(QNetworkReply*)), SLOT(slotAuthFailed(QNetworkReply*)));
    job->start();
    qDebug() << "# checking for authentication settings.";
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
        switch (_account->state()) {
        case Account::SignedOut:
            _account->setState(Account::SignedOut);
            break;
        default:
            _account->setState(Account::Disconnected);
        }

    } else if( reply->error() != QNetworkReply::NoError ) {
        _errors << reply->errorString();
    }

    emit connectionResult( stat );
}

void ConnectionValidator::slotAuthSuccess()
{
    _account->setState(Account::Connected);
    emit connectionResult(Connected);
}

} // namespace Mirall
