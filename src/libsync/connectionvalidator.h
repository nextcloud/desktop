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
        // actually also used for timeouts or errors on the authed request
        StatusNotFound
    };

    void checkConnection();

    static QString statusString( Status );
    static bool isNetworkError( Status status );

signals:
    void connectionResult( ConnectionValidator::Status status, QStringList errors );

protected slots:
    void slotStatusFound(const QUrl&url, const QVariantMap &info);
    void slotNoStatusFound(QNetworkReply *reply);
    void slotJobTimeout(const QUrl& url);

    void slotCheckAuthentication();
    void slotAuthFailed(QNetworkReply *reply);
    void slotAuthSuccess();

private:
    void reportResult(Status status);

    QStringList _errors;
    Account   *_account;
};

}

#endif // CONNECTIONVALIDATOR_H
