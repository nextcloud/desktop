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

#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H

#include "config.h"
#include <QObject>

#ifdef WITH_QTKEYCHAIN
#include "qtkeychain/keychain.h"

using namespace QKeychain;
#else
// FIXME: If the slot definition below is ifdefed for some reason the slot is
// not there even if WITH_QTKEYCHAIN is defined.
typedef void QKeychain::Job;
#endif


namespace Mirall {

/*
 * This object holds the credential information of the ownCloud connection. It
 * is implemented as a singleton.
 * At startup of the client, at first the fetchCredentials() method must be called
 * which tries to get credentials from one of the supported backends. To determine
 * which backend should be used, MirallConfigFile::credentialType() is called as
 * the backend is configured in the config file.
 *
 * The fetchCredentials() call changes the internal state of the credential store
 * to one of
 *   Ok: There are credentials. Note that it's unknown if they are correct!!
 *   UserCanceled: The fetching involved user interaction and the user canceled
 *                 the operation. No valid credentials are there.
 *   TooManyAttempts: The user tried to often to enter a password.
 *   Fetching: The fetching is not yet finished.
 *   Error:    A general error happened.
 * After fetching has finished, signal fetchCredentialsFinished(bool) is emitted.
 * The result can be retrieved with state() and password() and user() methods.
 */

class CredentialStore : public QObject
{
    Q_OBJECT
public:
    enum CredState { NotFetched = 0, Ok, UserCanceled, Fetching, AsyncFetching, Error, TooManyAttempts };

    QString password( const QString& connection = QString::null ) const;
    QString user( const QString& connection = QString::null ) const;

    /**
     * @brief state
     * @return the state of the Credentialstore.
     */
    CredState state();

    /**
     * @brief fetchCredentials - start to retrieve user credentials.
     *
     * This method must be called first to retrieve the credentials.
     * At the end, this method emits the fetchKeyChainFinished() signal.
     */
    void fetchCredentials();

    /**
     * @brief basicAuthHeader - return a basic authentication header.
     * @return a QByteArray with a ready to use Header for HTTP basic auth.
     */
    QByteArray basicAuthHeader() const;

    /**
     * @brief instance - singleton pointer.
     * @return the singleton pointer to access the object.
     */
    static CredentialStore *instance();

    /**
     * @brief setCredentials - sets the user credentials.
     *
     * This function is called from the setup wizard to set the credentials
     * int this store. Note that it does not store the password.
     * The function also sets the state to ok.
     * @param user - the user name
     * @param password - the password.
     */
    void setCredentials( const QString&, const QString& );
signals:
    /**
     * @brief fetchCredentialsFinished
     *
     * emitted as soon as the fetching of the credentials has finished.
     * If the parameter is true, there is a password and user. This does
     * however, not say if the credentials are valid log in data.
     * If false, the user pressed cancel.
     */
    void fetchCredentialsFinished(bool);

protected slots:
    void slotKeyChainFinished(QKeychain::Job* job);

private:
    explicit CredentialStore(QObject *parent = 0);

    static CredentialStore *_instance;
    static CredState _state;
    static QString _passwd;
    static QString _user;
    static int     _tries;
};
}

#endif // CREDENTIALSTORE_H
