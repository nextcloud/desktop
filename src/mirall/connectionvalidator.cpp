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
#include "mirall/credentialstore.h"

namespace Mirall {

ConnectionValidator::ConnectionValidator(QObject *parent) :
    QObject(parent)
{

}

ConnectionValidator::ConnectionValidator(const QString& connection, QObject *parent)
    :_connection(connection)
{
    ownCloudInfo::instance()->setCustomConfigHandle(_connection);
}

QStringList ConnectionValidator::errors() const
{
    return _errors;
}

QString ConnectionValidator::statusString( Status )
{
    return QLatin1String("Get your street creds!");
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
        emit connectionResult( NotConfigured );
    }
}

void ConnectionValidator::slotStatusFound( const QString& url, const QString& versionStr, const QString& version, const QString& edition)
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
        _errors.append( tr("<p>The configured server for this client is too old.</p>"
                           "<p>Please update to the latest server and restart the client.</p>"));
        emit connectionResult( ServerVersionMismatch );
        return;
    }

    QTimer::singleShot( 0, this, SLOT( slotFetchCredentials() ));
}

// status.php could not be loaded.
void ConnectionValidator::slotNoStatusFound(QNetworkReply *reply)
{
    // disconnect from ownCloudInfo
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                this, SLOT(slotStatusFound(QString,QString,QString,QString)));

    disconnect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotNoStatusFound(QNetworkReply*)));

    _errors.append( reply->errorString() );
    emit connectionResult( StatusNotFound );

}

void ConnectionValidator::slotFetchCredentials()
{
    if( _connection.isEmpty() ) {
        if( CredentialStore::instance()->canTryAgain() ) {
            connect( CredentialStore::instance(), SIGNAL(fetchCredentialsFinished(bool)),
                     this, SLOT(slotCredentialsFetched(bool)) );
            CredentialStore::instance()->fetchCredentials();
        }

        if( CredentialStore::instance()->state() == CredentialStore::TooManyAttempts ) {
            _errors << tr("Too many attempts to get a valid password.");
            emit connectionResult( CredentialsTooManyAttempts );
        }
    } else {
        // Pull credentials from Mirall config.
        slotCredentialsFetched( true );
    }
}

void ConnectionValidator::slotCredentialsFetched( bool ok )
{
    qDebug() << "Credentials successfully fetched: " << ok;
    disconnect( CredentialStore::instance(), SIGNAL(fetchCredentialsFinished(bool)) );

    if( ! ok ) {
        Status stat;
        _errors << tr("Error: Could not retrieve the password!");

        if( CredentialStore::instance()->state() == CredentialStore::UserCanceled ) {
            _errors << tr("Password dialog was canceled!");
            stat = CredentialsUserCanceled;
        } else {
            _errors << CredentialStore::instance()->errorMessage();
            stat = CredentialError;
        }

        qDebug() << "Could not fetch credentials" << _errors;

        emit connectionResult( stat );
    } else {
        QString user, pwd;
        if( _connection.isEmpty() ) {
            user = CredentialStore::instance()->user();
            pwd  = CredentialStore::instance()->password();
        } else {
            // in case of reconfiguration, the _connection is set.
            MirallConfigFile cfg(_connection);
            user = cfg.ownCloudUser();
            pwd  = cfg.ownCloudPasswd();
        }
        ownCloudInfo::instance()->setCredentials( user, pwd );

        // Credential fetched ok.
        QTimer::singleShot( 0, this, SLOT( slotCheckAuthentication() ));
    }
}

void ConnectionValidator::slotCheckAuthentication()
{
    connect( ownCloudInfo::instance(), SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this, SLOT(slotAuthCheck(QString,QNetworkReply*)));

    qDebug() << "# checking for authentication settings.";
    ownCloudInfo::instance()->getRequest(QLatin1String("/"), true ); // this call needs to be authenticated.
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
}

void ConnectionValidator::slotAuthCheck( const QString& ,QNetworkReply *reply )
{
    bool ok = true;
    Status stat = Connected;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ||
            reply->error() == QNetworkReply::OperationCanceledError ) { // returned if the user is wrong.
        qDebug() << "******** Password is wrong!";
        _errors << "The provided credentials are wrong.";
        stat = CredentialsWrong;
        ok = false;
    }

    // disconnect from ownCloud Info signals
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    emit connectionResult( stat );

}


}
