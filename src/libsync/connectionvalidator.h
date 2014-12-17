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

namespace OCC {

class Account;

class OWNCLOUDSYNC_EXPORT ConnectionValidator : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionValidator(Account *account, QObject *parent = 0);

    enum Status {
        Undefined,
        Connected,
        NotConfigured,
        ServerVersionMismatch,
        CredentialsWrong,
        StatusNotFound,
        // actually also used for other errors on the authed request
        Timeout
    };

    static QString statusString( Status );
    static bool isNetworkError( Status status );

public slots:
    /// Checks the server and the authentication.
    void checkServerAndAuth();

    /// Checks authentication only.
    void checkAuthentication();

signals:
    void connectionResult( ConnectionValidator::Status status, QStringList errors );

protected slots:
    void slotStatusFound(const QUrl&url, const QVariantMap &info);
    void slotNoStatusFound(QNetworkReply *reply);
    void slotJobTimeout(const QUrl& url);

    void slotAuthFailed(QNetworkReply *reply);
    void slotAuthSuccess();

private:
    void reportResult(Status status);

    QStringList _errors;
    Account   *_account;
};

}

#endif // CONNECTIONVALIDATOR_H
