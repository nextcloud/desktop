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

bool CredentialStore::canTryAgain()
{
    MirallConfigFile::CredentialType t;
    MirallConfigFile cfgFile;

    bool canDoIt = false;

    if( _state == NotFetched ) {
        return true;
    }
    t = cfgFile.credentialType();
    switch( t ) {
    case MirallConfigFile::User:
        canDoIt = true;
        break;
    case MirallConfigFile::Settings:
        break;
    case MirallConfigFile::KeyChain:
        break;
    default:
        break;
    }
    return canDoIt;
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
            job->setKey( keyChainKey( cfgFile.ownCloudUrl() ) );

            connect( job, SIGNAL(finished(QKeychain::Job*)), this,
                     SLOT(slotKeyChainReadFinished(QKeychain::Job*)));
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

void CredentialStore::reset()
{
    _state = NotFetched;
    _user   = QString::null;
    _passwd = QString::null;
    _tries = 0;
}

QString CredentialStore::keyChainKey( const QString& url ) const
{
    QString key = _user+QLatin1Char(':')+url;
    return key;
}

void CredentialStore::slotKeyChainReadFinished(QKeychain::Job* job)
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

void CredentialStore::setCredentials( const QString& url, const QString& user, const QString& pwd, bool noLocalPwd )
{
    _passwd = pwd;
    _user = user;
    _state = Ok;

#ifdef WITH_QTKEYCHAIN
    MirallConfigFile::CredentialType t;
    t = MirallConfigFile::KeyChain;
    if( noLocalPwd ) t = MirallConfigFile::User;

    switch( t ) {
    case MirallConfigFile::User:
        deleteKeyChainCredential(keyChainKey( url ));
        break;
    case MirallConfigFile::KeyChain: {
        // Set password in KeyChain
        WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
        // job->setAutoDelete( false );
        job->setKey( keyChainKey( url ) );
        job->setTextData(pwd);

        connect( job, SIGNAL(finished(QKeychain::Job*)), this,
                 SLOT(slotKeyChainWriteFinished(QKeychain::Job*)));
        job->start();

        break;
    }
    default:
        // unsupported.
        break;
    }
#else
    (void) url;
#endif
}

void CredentialStore::slotKeyChainWriteFinished( QKeychain::Job *job )
{
#ifdef WITH_QTKEYCHAIN
    WritePasswordJob *pwdJob = static_cast<WritePasswordJob*>(job);
    if( pwdJob ) {
        if( pwdJob->error() ) {
            qDebug() << "Error with keychain: " << pwdJob->errorString();
        } else {
            qDebug() << "Successfully stored password for user " << _user;
            // Try to remove password formerly stored in the config file.
            MirallConfigFile cfgFile;
            cfgFile.clearPasswordFromConfig();
        }
    } else {
        qDebug() << "Error: KeyChain Write Password Job failed!";
    }
#else
    (void) job;
#endif
}

// Called if a user chooses to not store the password locally.
void CredentialStore::deleteKeyChainCredential( const QString& key )
{
#ifdef WITH_QTKEYCHAIN
    // Start the remove job, do not care so much about the result.
    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setKey( key );
    job->start();
#endif
}

}
