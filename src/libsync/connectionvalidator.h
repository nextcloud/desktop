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

#ifndef CONNECTIONVALIDATOR_H
#define CONNECTIONVALIDATOR_H

#include "owncloudlib.h"
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QNetworkReply>
#include "accountfwd.h"

namespace OCC {

/**
 * This is a job-like class to check that the server is up and that we are connected.
 * There is two entry point: checkServerAndAuth and checkAuthentication
 * checkAutentication is the quick version that only do the propfind
 * while checkServerAndAuth is doing the 3 calls.
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
        +-> slotStatusFound
                credential->fetch() --+
                                      |
  +-----------------------------------+
  |
*-+-> checkAuthentication (PROPFIND on root)
        PropfindJob
        |
        +-> slotAuthFailed --> X
        |
        +-> slotAuthSuccess --+--> X (depending if comming from checkServerAndAuth or not)
                              |
  +---------------------------+
  |
  +-> checkServerCapabilities (cloud/capabilities)
        JsonApiJob
        |
        +-> slotCapabilitiesRecieved --> X
    \endcode
 */
class OWNCLOUDSYNC_EXPORT ConnectionValidator : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionValidator(AccountPtr account, QObject *parent = 0);

    enum Status {
        Undefined,
        Connected,
        NotConfigured,
        ServerVersionMismatch,
        CredentialsWrong,
        StatusNotFound,
        UserCanceledCredentials,
        ServiceUnavailable,
        // actually also used for other errors on the authed request
        Timeout
    };

    static QString statusString( Status );

public slots:
    /// Checks the server and the authentication.
    void checkServerAndAuth();
    void systemProxyLookupDone(const QNetworkProxy &proxy);

    /// Checks authentication only.
    void checkAuthentication();

signals:
    void connectionResult( ConnectionValidator::Status status, QStringList errors );

protected slots:
    void slotCheckServerAndAuth();

    void slotStatusFound(const QUrl&url, const QVariantMap &info);
    void slotNoStatusFound(QNetworkReply *reply);
    void slotJobTimeout(const QUrl& url);

    void slotAuthFailed(QNetworkReply *reply);
    void slotAuthSuccess();

    void slotCapabilitiesRecieved(const QVariantMap&);

private:
    void reportResult(Status status);
    void checkServerCapabilities();

    QStringList _errors;
    AccountPtr   _account;
    bool _isCheckingServerAndAuth;
};

}

#endif // CONNECTIONVALIDATOR_H
