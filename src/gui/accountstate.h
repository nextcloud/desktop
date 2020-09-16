/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */


#ifndef ACCOUNTINFO_H
#define ACCOUNTINFO_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QPointer>
#include "connectionvalidator.h"
#include "creds/abstractcredentials.h"
#include <memory>

class QSettings;

namespace OCC {

class AccountState;
class Account;
class AccountApp;
class RemoteWipe;
class VirtualDriveInterface;

using AccountStatePtr = QExplicitlySharedDataPointer<AccountState>;
using AccountAppList = QList<AccountApp *>;

/**
 * @brief Extra info about an ownCloud server account.
 * @ingroup gui
 */
class AccountState : public QObject, public QSharedData
{
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

        /// Similar to ServiceUnavailable, but we know the server is down
        /// for maintenance
        MaintenanceMode,

        /// Could not communicate with the server for some reason.
        /// We assume this may resolve itself over time and will try
        /// again automatically.
        NetworkError,

        /// Server configuration error. (For example: unsupported version)
        ConfigurationError,

        /// We are currently asking the user for credentials
        AskingCredentials
    };

    /// The actual current connectivity status.
    using ConnectionStatus = ConnectionValidator::Status;

    /// Use the account as parent
    explicit AccountState(AccountPtr account);
    ~AccountState();

    /** Creates an account state from settings and an Account object.
     *
     * Use from AccountManager with a prepared QSettings object only.
     */
    static AccountState *loadFromSettings(AccountPtr account, QSettings &settings);

    /** Writes account state information to settings.
     *
     * It does not write the Account data.
     */
    void writeToSettings(QSettings &settings);

    AccountPtr account() const;
    // FIXME: Can we get this down to Account at some point?
    VirtualDriveInterface *drive() const;

    ConnectionStatus connectionStatus() const;
    QStringList connectionErrors() const;

    State state() const;
    static QString stateString(State state);

    bool isSignedOut() const;

    AccountAppList appList() const;
    AccountApp* findApp(const QString &appId) const;

    /** A user-triggered sign out which disconnects, stops syncs
     * for the account and forgets the password. */
    void signOutByUi();

    /** Tries to connect from scratch.
     *
     * Does nothing for signed out accounts.
     * Connected accounts will be disconnected and try anew.
     * Disconnected accounts will go to checkConnectivity().
     *
     * Useful for when network settings (proxy) change.
     */
    void freshConnectionAttempt();

    /// Move from SignedOut state to Disconnected (attempting to connect)
    void signIn();

    bool isConnected() const;

    /** Returns a new settings object for this account, already in the right groups. */
    std::unique_ptr<QSettings> settings();

    /** Mark the timestamp when the last successful ETag check happened for
     *  this account.
     *  The checkConnectivity() method uses the timestamp to save a call to
     *  the server to validate the connection if the last successful etag job
     *  was not so long ago.
     */
    void tagLastSuccessfullETagRequest();

    /** Saves the ETag Response header from the last Notifications api
     * request with statusCode 200.
    */
    QByteArray notificationsEtagResponseHeader() const;

    /** Returns the ETag Response header from the last Notifications api
     * request with statusCode 200.
    */
    void setNotificationsEtagResponseHeader(const QByteArray &value);

    /** Saves the ETag Response header from the last Navigation Apps api
     * request with statusCode 200.
    */
    QByteArray navigationAppsEtagResponseHeader() const;

    /** Returns the ETag Response header from the last Navigation Apps api
     * request with statusCode 200.
    */
    void setNavigationAppsEtagResponseHeader(const QByteArray &value);

    ///Asks for user credentials
    void handleInvalidCredentials();

public slots:
    /// Triggers a ping to the server to update state and
    /// connection status and errors.
    void checkConnectivity();

private:
    void setState(State state);
    void fetchNavigationApps();

signals:
    void stateChanged(int state);
    void isConnectedChanged();
    void hasFetchedNavigationApps();

protected Q_SLOTS:
    void slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList &errors);

    /// When client gets a 401 or 403 checks if server requested remote wipe
    /// before asking for user credentials again
    void slotHandleRemoteWipeCheck();

    void slotCredentialsFetched(AbstractCredentials *creds);
    void slotCredentialsAsked(AbstractCredentials *creds);

    void slotNavigationAppsFetched(const QJsonDocument &reply, int statusCode);
    void slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode);
    void slotOcsError(int statusCode, const QString &message);

private:
    AccountPtr _account;
    VirtualDriveInterface *_drive = nullptr;
    State _state;
    ConnectionStatus _connectionStatus;
    QStringList _connectionErrors;
    bool _waitingForNewCredentials;
    QElapsedTimer _timeSinceLastETagCheck;
    QPointer<ConnectionValidator> _connectionValidator;
    QByteArray _notificationsEtagResponseHeader;
    QByteArray _navigationAppsEtagResponseHeader;

    /**
     * Starts counting when the server starts being back up after 503 or
     * maintenance mode. The account will only become connected once this
     * timer exceeds the _maintenanceToConnectedDelay value.
     */
    QElapsedTimer _timeSinceMaintenanceOver;

    /**
     * Milliseconds for which to delay reconnection after 503/maintenance.
     */
    int _maintenanceToConnectedDelay;

    /**
     * Connects remote wipe check with the account
     * the log out triggers the check (loads app password -> create request)
     */
    RemoteWipe *_remoteWipe;

    /**
     * Holds the App names and URLs available on the server
     */
    AccountAppList _apps;

};

class AccountApp : public QObject
{
    Q_OBJECT
public:
    AccountApp(const QString &name, const QUrl &url,
        const QString &id, const QUrl &iconUrl,
        QObject* parent = nullptr);

    QString name() const;
    QUrl url() const;
    QString id() const;
    QUrl iconUrl() const;

private:
    QString _name;
    QUrl _url;

    QString _id;
    QUrl _iconUrl;
};

}

Q_DECLARE_METATYPE(OCC::AccountState *)
Q_DECLARE_METATYPE(OCC::AccountStatePtr)

#endif //ACCOUNTINFO_H
