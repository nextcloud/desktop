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
#include <QPointer>
#include "utility.h"
#include "connectionvalidator.h"
#include "creds/abstractcredentials.h"
#include <memory>

class QSettings;

namespace OCC {

class AccountState;
class Account;

/**
 * @brief Extra info about an ownCloud server account.
 * @ingroup gui
 */
class AccountState : public QObject {
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

        /// There's a temporary problem with talking to the server,
        /// don't bother the user too much and try again.
        ServiceUnavailable,

        /// Could not communicate with the server for some reason.
        /// We assume this may resolve itself over time and will try
        /// again automatically.
        NetworkError,

        /// An error like invalid credentials where retrying won't help.
        ConfigurationError
    };
    enum CredentialFetchMode { Interactive, NonInteractive };

    /// The actual current connectivity status.
    typedef ConnectionValidator::Status ConnectionStatus;

    /// Use the account as parent
    AccountState(AccountPtr account);
    ~AccountState();

    AccountPtr account() const;

    ConnectionStatus connectionStatus() const;
    QStringList connectionErrors() const;
    static QString connectionStatusString(ConnectionStatus status);

    State state() const;
    static QString stateString(State state);

    bool isSignedOut() const;
    void setSignedOut(bool signedOut);

    bool isConnected() const;
    bool isConnectedOrTemporarilyUnavailable() const;

    /// Triggers a ping to the server to update state and
    /// connection status and errors.
    void checkConnectivity(CredentialFetchMode credentialsFetchMode);

    /** Returns a new settings object for this account, already in the right groups. */
    std::unique_ptr<QSettings> settings();

    /** display name with two lines that is displayed in the settings
     * If width is bigger than 0, the string will be ellided so it does not exceed that width
     */
    QString shortDisplayNameForSettings(int width = 0) const;

    /** Mark the timestamp when the last successful ETag check happened for
     *  this account.
     *  The checkConnectivity() method uses the timestamp to save a call to
     *  the server to validate the connection if the last successful etag job
     *  was not so long ago.
     */
    void tagLastSuccessfullETagRequest();

private:
    void setState(State state);

signals:
    void stateChanged(int state);

protected Q_SLOTS:
    void slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList& errors);
    void slotInvalidCredentials();
    void slotCredentialsFetched(AbstractCredentials* creds);
    void slotCredentialsAsked(AbstractCredentials* creds);

private:
    AccountPtr _account;
    State _state;
    ConnectionStatus _connectionStatus;
    QStringList _connectionErrors;
    bool _waitingForNewCredentials;
    CredentialFetchMode _credentialsFetchMode;
    QElapsedTimer _timeSinceLastETagCheck;
    QPointer<ConnectionValidator> _connectionValidator;
};

}

Q_DECLARE_METATYPE(OCC::AccountState*)
Q_DECLARE_METATYPE(QSharedPointer<OCC::AccountState>)

#endif //ACCOUNTINFO_H
