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
#include <QInputDialog>

#include "config.h"

#include "mirall/credentialstore.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"

#ifdef WITH_QTKEYCHAIN
#include <qtkeychain/keychain.h>
using namespace QKeychain;
#endif

#define MAX_LOGIN_ATTEMPTS 3

namespace Mirall {

CredentialStore *CredentialStore::_instance=0;
CredentialStore::CredState CredentialStore::_state = NotFetched;
QString CredentialStore::_passwd   = QString::null;
QString CredentialStore::_user     = QString::null;
QString CredentialStore::_url      = QString::null;
QString CredentialStore::_errorMsg = QString::null;
int     CredentialStore::_tries    = 0;
#ifdef WITH_QTKEYCHAIN
CredentialStore::CredentialType CredentialStore::_type = KeyChain;
#else
CredentialStore::CredentialType CredentialStore::_type = Settings;
#endif

CredentialStore::CredentialStore(QObject *parent) :
    QObject(parent)
{
}

CredentialStore *CredentialStore::instance()
{
    if( !CredentialStore::_instance ) CredentialStore::_instance = new CredentialStore;
    return CredentialStore::_instance;
}

QString CredentialStore::password() const
{
    return _passwd;
}
QString CredentialStore::user() const
{
    return _user;
}

CredentialStore::CredState CredentialStore::state()
{
    return _state;
}

bool CredentialStore::canTryAgain()
{
    bool canDoIt = false;

    if( _tries > MAX_LOGIN_ATTEMPTS ) {
        qDebug() << "canTryAgain: Max attempts reached.";
        return false;
    }

    if( _state == NotFetched ) {
        return true;
    }

    switch( _type ) {
    case CredentialStore::User:
        canDoIt = true;
        break;
    case CredentialStore::Settings:
        break;
    case CredentialStore::KeyChain:
        canDoIt = true;
        break;
    default:
        break;
    }
    return canDoIt;
}

void CredentialStore::fetchCredentials()
{
    MirallConfigFile cfgFile;
    if( ++_tries > MAX_LOGIN_ATTEMPTS ) {
        qDebug() << "Too many attempts to enter password!";
        _state = TooManyAttempts;
        emit( fetchCredentialsFinished(false) );
        return;
    }

    bool ok = false;
    QString pwd;
    _user = cfgFile.ownCloudUser();
    _url  = cfgFile.ownCloudUrl();
    if( !cfgFile.passwordStorageAllowed() ) {
        _type = CredentialStore::User;
    }

    QString key = keyChainKey(_url);

    if( key.isNull() ) {
        qDebug() << "Can not fetch credentials, url is zero!";
        _state = Error;
        emit( fetchCredentialsFinished(false) );
        return;
    }

    switch( _type ) {
    case CredentialStore::User: {
        /* Ask the user for the password */
        /* Fixme: Move user interaction out here. */
        _state = AsyncFetching;
        _inputDialog = new QInputDialog;
        _inputDialog->setWindowTitle(QApplication::translate("MirallConfigFile","Password Required") );
        _inputDialog->setLabelText( QApplication::translate("MirallConfigFile","Please enter your %1 password:")
                .arg(Theme::instance()->appNameGUI()));
        _inputDialog->setInputMode( QInputDialog::TextInput );
        _inputDialog->setTextEchoMode( QLineEdit::Password );

        connect(_inputDialog, SIGNAL(finished(int)), SLOT(slotUserDialogDone(int)));
        _inputDialog->exec();
        break;
    }
    case CredentialStore::Settings: {
        /* Read from config file. */
        _state = Fetching;
        pwd = cfgFile.ownCloudPasswd();
        ok = true;
        break;
    }
    case CredentialStore::KeyChain: {
        // If the credentials are here already, return.
        if( _state == Ok ) {
            emit(fetchCredentialsFinished(true));
            return;
        }
        // otherwise fetch asynchronious.
#ifdef WITH_QTKEYCHAIN
        _state = AsyncFetching;
        if( !_user.isEmpty() ) {
            ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
            job->setKey( key );

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

void CredentialStore::slotUserDialogDone( int result )
{
    if( result == QDialog::Accepted ) {
        _passwd = _inputDialog->textValue();
        _state = Ok;
    } else {
        _state = UserCanceled;
        _passwd = QString::null;
    }
    _inputDialog->deleteLater();
    emit(fetchCredentialsFinished(_state == Ok));
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
    QString u(url);
    if( u.isEmpty() ) {
        qDebug() << "Empty url in keyChain, error!";
        return QString::null;
    }
    if( _user.isEmpty() ) {
        qDebug() << "Error: User is emty!";
        return QString::null;
    }

    if( !u.endsWith(QChar('/')) ) {
        u.append(QChar('/'));
    }

    QString key = _user+QLatin1Char(':')+u;
    return key;
}

void CredentialStore::slotKeyChainReadFinished(QKeychain::Job* job)
{
#ifdef WITH_QTKEYCHAIN
    ReadPasswordJob *pwdJob = static_cast<ReadPasswordJob*>(job);
    if( pwdJob ) {
        switch( pwdJob->error() ) {
        case QKeychain::NoError:
            _passwd = pwdJob->textData();
#ifdef Q_OS_LINUX
            // Currently there is a bug in the keychain on linux that if no
            // entry is there, an empty password comes back, but no error.
            if( _passwd.isEmpty() ) {
                _state = EntryNotFound;
                _errorMsg = tr("No password entry found in keychain. Please reconfigure.");
            } else
#endif
            _state = Ok;
            break;
        case QKeychain::EntryNotFound:
            _state = EntryNotFound;
            break;
        case QKeychain::CouldNotDeleteEntry:
            _state = Error;
            break;
        case QKeychain::AccessDeniedByUser:
           _state = AccessDeniedByUser;
            break;
        case QKeychain::AccessDenied:
            _state = AccessDenied;
            break;
        case QKeychain::NoBackendAvailable:
            _state = NoKeychainBackend;
            break;
        case QKeychain::NotImplemented:
            _state = NoKeychainBackend;
            break;
        case QKeychain::OtherError:
        default:
            _state = Error;

        }
        /* In case there is no backend, tranparentely switch to Settings file. */
        if( _state == NoKeychainBackend ) {
            qDebug() << "No Storage Backend, falling back to Settings mode.";
            _type = CredentialStore::Settings;
            fetchCredentials();
            return;
        }

        if( _state == EntryNotFound ) {
            // try to migrate.
        }

        if( _state != Ok ) {
            qDebug() << "Error with keychain: " << pwdJob->errorString();
            if(_errorMsg.isEmpty()) _errorMsg = pwdJob->errorString();
        } else {
            _errorMsg = QString::null;
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

QString CredentialStore::errorMessage()
{
    return _errorMsg;
}

void CredentialStore::setCredentials( const QString& url, const QString& user,
                                      const QString& pwd, bool allowToStore )
{
    _passwd = pwd;
    _user = user;
    if( allowToStore ) {
#ifdef WITH_QTKEYCHAIN
        _type = KeyChain;
#else
        _type = Settings;
#endif
    } else {
        _type = User;
    }
    _url  = url;
    _state = Ok;
}

void CredentialStore::saveCredentials( )
{
    MirallConfigFile cfgFile;
    QString key = keyChainKey(_url);
    if( key.isNull() ) {
        qDebug() << "Error: Can not save credentials, URL is zero!";
        return;
    }
#ifdef WITH_QTKEYCHAIN
    WritePasswordJob *job = NULL;
#endif

    switch( _type ) {
    case CredentialStore::User:
        deleteKeyChainCredential( key );
        break;
    case CredentialStore::KeyChain:
#ifdef WITH_QTKEYCHAIN
        // Set password in KeyChain
        job = new WritePasswordJob(Theme::instance()->appName());
        job->setKey( key );
        job->setTextData(_passwd);

        connect( job, SIGNAL(finished(QKeychain::Job*)), this,
                 SLOT(slotKeyChainWriteFinished(QKeychain::Job*)));
        job->start();
#endif
        break;
    case CredentialStore::Settings:
        cfgFile.writePassword( _passwd );
        reset();
        break;
    default:
        // unsupported.
        break;
    }
}

void CredentialStore::slotKeyChainWriteFinished( QKeychain::Job *job )
{
#ifdef WITH_QTKEYCHAIN
    WritePasswordJob *pwdJob = static_cast<WritePasswordJob*>(job);
    if( pwdJob ) {
        QKeychain::Error err = pwdJob->error();

        if( err != QKeychain::NoError ) {
            qDebug() << "Error with keychain: " << pwdJob->errorString();
            if( err == NoBackendAvailable || err == NotImplemented ||
                    pwdJob->errorString().contains(QLatin1String("Could not open wallet"))) {
                _type = Settings;
                saveCredentials();
            }
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
