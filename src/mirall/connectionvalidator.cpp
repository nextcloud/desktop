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

#include "mirall/connectionvalidator.h"
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"

namespace Mirall {

ConnectionValidator::ConnectionValidator(QObject *parent) :
    QObject(parent)
{

}

ConnectionValidator::ConnectionValidator(const QString& connection, QObject *parent)
    : QObject(parent),
      _connection(connection),
      _networkError(QNetworkReply::NoError)
{
    ownCloudInfo::instance()->setCustomConfigHandle(_connection);
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
    if( ownCloudInfo::instance()->isConfigured() ) {
        connect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                 SLOT(slotStatusFound(QString,QString,QString,QString)));

        connect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                 SLOT(slotNoStatusFound(QNetworkReply*)));

        // checks for status.php
        ownCloudInfo::instance()->checkInstallation();
    } else {
        _errors << tr("No ownCloud connection configured");
        emit connectionResult( NotConfigured );
    }
}

void ConnectionValidator::slotStatusFound( const QString& url, const QString& versionStr, const QString& version, const QString& /*edition*/)
{
    // status.php was found.
    qDebug() << "** Application: ownCloud found: " << url << " with version " << versionStr << "(" << version << ")";
    // now check the authentication
    MirallConfigFile cfgFile(_connection);

    cfgFile.setOwnCloudVersion( version );
    // disconnect from ownCloudInfo
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                this, SLOT(slotStatusFound(QString,QString,QString,QString)));

    disconnect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotNoStatusFound(QNetworkReply*)));

    if( version.startsWith("4.0") ) {
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
    // disconnect from ownCloudInfo
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                this, SLOT(slotStatusFound(QString,QString,QString,QString)));

    disconnect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotNoStatusFound(QNetworkReply*)));

    _errors.append(tr("Unable to connect to %1").arg(reply->url().toString()));
    _errors.append( reply->errorString() );
    _networkError = (reply->error() != QNetworkReply::NoError);
    emit connectionResult( StatusNotFound );

}

void ConnectionValidator::slotCheckAuthentication()
{
    connect( ownCloudInfo::instance(), SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this, SLOT(slotAuthCheck(QString,QNetworkReply*)));

    qDebug() << "# checking for authentication settings.";
    ownCloudInfo::instance()->getWebDAVPath(QLatin1String("/") ); // this call needs to be authenticated.
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
}

void ConnectionValidator::slotAuthCheck( const QString&, QNetworkReply *reply )
{
    Status stat = Connected;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ||
            reply->error() == QNetworkReply::OperationCanceledError ) { // returned if the user is wrong.
        qDebug() << "******** Password is wrong!";
        _errors << tr("The provided credentials are not correct");
        stat = CredentialsWrong;
    } else if( reply->error() != QNetworkReply::NoError ) {
        _errors << reply->errorString();
    }

    // disconnect from ownCloud Info signals
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    emit connectionResult( stat );
}


}
