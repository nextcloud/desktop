/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef CONNECTIONVALIDATOR_H
#define CONNECTIONVALIDATOR_H

#include "accountfwd.h"
#include "owncloudlib.h"

#include <QNetworkReply>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <chrono>

namespace OCC {

/**
 * This is a job-like class to check that the server is up and that we are connected.
 * There are two entry points: checkServerAndAuth and checkAuthentication
 * checkAuthentication is the quick version that only does the propfind
 * while checkServerAndAuth is doing the 4 calls.
 *
 * We cannot use the capabilites call to test the login and the password because of
 * https://github.com/owncloud/core/issues/12930
 *
 * Here follows the state machine

\code{.unparsed}
*---> checkServerAndAuth  (check status.php)
        Will asynchronously check for system proxy (if using system proxy)
        And then invoke slotCheckServerAndAuth
        CheckServerJob
        |
        +-> slotNoStatusFound --> X
        |
        +-> slotJobTimeout --> X
        |
        +-> slotStatusFound --+--> X (if credentials are still missing)
                              |
  +---------------------------+
  |
*-+-> checkAuthentication (PROPFIND on root)
        PropfindJob
        |
        +-> slotAuthFailed --> X
        |
        +-> slotAuthSuccess --+--> X (depending if coming from checkServerAndAuth or not)
                              |
  +---------------------------+
  |
  +-> checkServerCapabilities
        JsonApiJob (cloud/capabilities) -> slotCapabilitiesRecieved -+
                                                                     |
  +------------------------------------------------------------------+
  |
  +-> fetchUser -+
                 |
                 +-> AvatarJob
                            |
                            +-> slotAvatarImage --> reportResult()

    \endcode
 */
class ConnectionValidator : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionValidator(AccountPtr account, QObject *parent = nullptr);
    enum class ValidationMode {
        ValidateServer,
        ValidateAuth,
        ValidateAuthAndUpdate
    };

    enum Status {
        Undefined,
        Connected,
        NotConfigured,
        ServerVersionMismatch, // The server version is too old
        CredentialsNotReady, // Credentials aren't ready
        CredentialsWrong, // AuthenticationRequiredError
        SslError, // SSL handshake error, certificate rejected by user?
        StatusNotFound, // Error retrieving status.php
        ServiceUnavailable, // 503 on authed request
        MaintenanceMode, // maintenance enabled in status.php
        Timeout // actually also used for other errors on the authed request
    };
    Q_ENUM(Status);

    // How often should the Application ask this object to check for the connection?
    static constexpr auto DefaultCallingInterval = std::chrono::seconds(62);


    /** Whether to clear the cookies before we start the CheckServerJob job
     * This option also depends on Theme::instance()->connectionValidatorClearCookies()
     */
    void setClearCookies(bool clearCookies);

public slots:
    /// Checks the server and the authentication.
    void checkServer(ConnectionValidator::ValidationMode mode = ConnectionValidator::ValidationMode::ValidateAuthAndUpdate);

    void systemProxyLookupDone(const QNetworkProxy &proxy);

signals:
    void connectionResult(ConnectionValidator::Status status, const QStringList &errors);

    void sslErrors(const QList<QSslError> &errors);

protected slots:
    /// Checks authentication only.
    void checkAuthentication();
    void slotCheckServerAndAuth();

    void slotStatusFound(const QUrl &url, const QJsonObject &info);

    void slotAuthFailed(QNetworkReply *reply);
    void slotAuthSuccess();

#ifndef TOKEN_AUTH_ONLY
    void slotAvatarImage(const QPixmap &img);
#endif

private:
    void reportResult(Status status);
    void checkServerCapabilities();
    void fetchUser();

    /** Sets the account's server version
     *
     * Returns false and reports ServerVersionMismatch for very old servers.
     */
    bool setAndCheckServerVersion(const QString &version);

    QStringList _errors;
    AccountPtr _account;
    bool _clearCookies = false;

    ConnectionValidator::ValidationMode _mode = ConnectionValidator::ValidationMode::ValidateAuthAndUpdate;
};
}


#endif // CONNECTIONVALIDATOR_H
