/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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


#ifndef ACCOUNTINFO_H
#define ACCOUNTINFO_H

#include <QByteArray>
#include "utility.h"
#include "connectionvalidator.h"


namespace OCC {

class QuotaInfo;
class AccountState;
class Account;
class AbstractCredentials;

class OWNCLOUDSYNC_EXPORT AccountStateManager : public QObject {
    Q_OBJECT
public:
    static AccountStateManager *instance();

    AccountStateManager();
    ~AccountStateManager();

    AccountState *accountState() { return _accountState; }

signals:
    void accountStateAdded(AccountState *accountState);
    void accountStateRemoved(AccountState *accountState);

private slots:
    void slotAccountAdded(Account *account);

private:
    void setAccountState(AccountState *account);

    AccountState *_accountState;
};

/**
 * @brief Extra info about an ownCloud server account.
 */
class OWNCLOUDSYNC_EXPORT AccountState : public QObject {
    Q_OBJECT
public:
    enum State {
        /// Not even attempting to connect, most likely because the
        /// user explicitly signed out or cancelled a credential dialog.
        SignedOut,

        /// Account would like to be connected but hasn't heard back yet.
        Disconnected,

        /// The account is successfully talking to the server.
        Connected,

        /// Could not communicate with the server for some reason.
        /// We assume this may resolve itself over time and will try
        /// again automatically.
        NetworkError,

        /// An error like invalid credentials where retrying won't help.
        ConfigurationError
    };

    /// The actual current connectivity status.
    typedef ConnectionValidator::Status ConnectionStatus;

    AccountState(Account* account);
    ~AccountState();

    Account* account() const;

    ConnectionStatus connectionStatus() const;
    QStringList connectionErrors() const;
    static QString connectionStatusString(ConnectionStatus status);

    State state() const;
    static QString stateString(State status);

    bool isSignedOut() const;
    void setSignedOut(bool signedOut);

    bool isConnected() const;

    QuotaInfo *quotaInfo();

    /// Triggers a ping to the server to update state and
    /// connection status and errors.
    void checkConnectivity();

private:
    void setState(State state);

signals:
    void stateChanged(int state);

protected Q_SLOTS:
    void slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList& errors);
    void slotInvalidCredentials();
    void slotCredentialsFetched(AbstractCredentials* creds);

private:
    Account * _account;
    QuotaInfo *_quotaInfo;
    State _state;
    ConnectionStatus _connectionStatus;
    QStringList _connectionErrors;
    bool _waitingForNewCredentials;
};

}

Q_DECLARE_METATYPE(OCC::AccountState*)

#endif //ACCOUNTINFO_H
