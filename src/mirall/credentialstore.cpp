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

#include "config.h"

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
#ifdef WITH_QTKEYCHAIN
        _state = AsyncFetching;
        if( !_user.isEmpty() ) {
            ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
            // job->setAutoDelete( false );
            job->setKey( _user );

            connect( job, SIGNAL(finished(QKeychain::Job*)), this,
                     SLOT(slotKeyChainFinished(QKeychain::Job*)));
            job->start();
        }
#else
        qDebug() << "QtKeyChain: Not yet implemented!";
        _state = Error;
#endif
        break;
    }
    default: {
        break;
    }
    }

    if( _state == Fetching ) { // ...but not AsyncFetching
        if( ok ) {
            _passwd = pwd;
            _state = Ok;
        }
        if( !ok && _state == Fetching ) {
            _state = Error;
        }

        emit( fetchCredentialsFinished(ok) );
    } else {
        // in case of AsyncFetching nothing happens here. The finished-Slot
        // will emit the finish signal.
    }
}

void CredentialStore::slotKeyChainFinished(QKeychain::Job* job)
{
#ifdef WITH_QTKEYCHAIN
    ReadPasswordJob *pwdJob = static_cast<ReadPasswordJob*>(job);
    if( pwdJob ) {
        if( pwdJob->error() ) {
            qDebug() << "Error with keychain: " << pwdJob->errorString();
            _state = Error;
        } else {
            _passwd = pwdJob->textData();
            _state = Ok;
        }
    } else {
        _state = Error;
        qDebug() << "Error: KeyChain Read Password Job failed!";
    }
    emit(fetchCredentialsFinished(_state == Ok));
#else
    (void) job;
#endif
}


QByteArray CredentialStore::basicAuthHeader() const
{
    QString concatenated = _user + QLatin1Char(':') + _passwd;
    const QString b(QLatin1String("Basic "));
    QByteArray data = b.toLocal8Bit() + concatenated.toLocal8Bit().toBase64();

    return data;
}

void CredentialStore::setCredentials( const QString& user, const QString& pwd )
{
    _passwd = pwd;
    _user = user;
    _state = Ok;
}

}
