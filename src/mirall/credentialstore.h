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

#include <QObject>
#include <QInputDialog>

namespace QKeychain {
  class Job;
}

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
    enum CredState { NotFetched = 0,
                     Ok,
                     UserCanceled,
                     Fetching,
                     AsyncFetching,
                     EntryNotFound,
                     AccessDeniedByUser,
                     AccessDenied,
                     NoKeychainBackend,
                     Error,
                     TooManyAttempts };

    enum CredentialType {
        User = 0,
        Settings,
        KeyChain
    };

    QString password( ) const;
    QString user( ) const;

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
     * @param url - the connection url
     * @param user - the user name
     * @param password - the password.
     */
    void setCredentials( const QString&, const QString&, const QString&, bool );

    void saveCredentials( );

    QString errorMessage();

    /**
     * @brief canTryAgain - check if another try to get credentials makes sense.
    */
    bool canTryAgain();

    void reset();
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
    void slotKeyChainReadFinished( QKeychain::Job* );
    void slotKeyChainWriteFinished( QKeychain::Job* );
    void slotUserDialogDone(int);

private:
    explicit CredentialStore(QObject *parent = 0);
    void deleteKeyChainCredential( const QString& );
    QString keyChainKey( const QString& ) const;

    static CredentialStore *_instance;
    static CredState _state;
    static QString _passwd;
    static QString _user;
    static QString _url;
    static QString _errorMsg;
    static int     _tries;
    static CredentialType _type;
    QInputDialog   *_inputDialog;
};
}

#endif // CREDENTIALSTORE_H
