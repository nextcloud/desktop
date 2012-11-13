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

#include <QtGui>

#include "mirall/credentialstore.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"

namespace Mirall {

CredentialStore *CredentialStore::_instance=0;
CredentialStore::CredState CredentialStore::_state = NotFetched;
QString CredentialStore::_passwd = QString::null;
QString CredentialStore::_user   = QString::null;
int     CredentialStore::_tries  = 0;

CredentialStore::CredentialStore(QObject *parent) :
    QObject(parent)
{
}

CredentialStore *CredentialStore::instance()
{
    if( !CredentialStore::_instance ) CredentialStore::_instance = new CredentialStore;
    return CredentialStore::_instance;
}

QString CredentialStore::password( const QString& ) const
{
    return _passwd;
}
QString CredentialStore::user( const QString& ) const
{
    return _user;
}

CredentialStore::CredState CredentialStore::state()
{
    return _state;
}

void CredentialStore::fetchCredentials()
{
    _state = Fetching;
    MirallConfigFile cfgFile;
    MirallConfigFile::CredentialType t;

    if( _tries++ == 3 ) {
        qDebug() << "Too many attempts to enter password!";
        _state = TooManyAttempts;
        return;
    }
    t = cfgFile.credentialType();

    bool ok = false;
    QString pwd;
    _state = Fetching;
    _user = cfgFile.ownCloudUser();

    switch( t ) {
    case MirallConfigFile::User: {
        /* Ask the user for the password */
        /* Fixme: Move user interaction out here. */
        pwd = QInputDialog::getText(0, QApplication::translate("MirallConfigFile","Password Required"),
                                    QApplication::translate("MirallConfigFile","Please enter your %1 password:")
                                    .arg(Theme::instance()->appName()),
                                    QLineEdit::Password,
                                    QString::null, &ok);
        if( !ok ) {
            _state = UserCanceled;
        }
        break;
    }
    case MirallConfigFile::Settings: {
        /* Read from config file. */
        pwd = cfgFile.ownCloudPasswd();
        ok = true;
        break;
    }
    case MirallConfigFile::KeyChain: {
        /* Qt Keychain is not yet implemented. */
#ifdef HAVE_QTKEYCHAIN
        if( !_user.isEmpty() ) {
            ReadPasswordJoei b job( QLatin1String(Theme::instance()->appName()) );
            job.setAutoDelete( false );
            job.setKey( _user );

            job.connect( &job, SIGNAL(finished(QKeychain::Job*)), this,
                         SLOT(slotKeyChainFinished(QKeyChain::Job*)));
            job.start();
        }
#else
        qDebug() << "QtKeyChain: Not yet implemented!";
#endif
        break;
    }
    default: {
        break;
    }
    }

    if( ok ) {
        _passwd = pwd;
        _state = Ok;
    }
    if( !ok && _state == Fetching ) {
        _state = Error;
    }

    emit( fetchCredentialsFinished(ok) );
}

#ifdef HAVE_QTKEYCHAIN
void CredentialsStore::slotKeyChainFinished(QKeyChain::Job* job)
{
    if( job ) {
        if( job->error() ) {
            qDebug() << "Error mit keychain: " << job->errorString();
        } else {
            _passwd = job.textData();
        }
    }
}
#endif


QByteArray CredentialStore::basicAuthHeader() const
{
    QString concatenated = _user + QLatin1Char(':') + _passwd;
    const QString b(QLatin1String("Basic "));
    QByteArray data = b.toLocal8Bit() + concatenated.toLocal8Bit().toBase64();

    return data;
}

}
