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

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QNetworkReply>

namespace Mirall {

class Account;

class ConnectionValidator : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionValidator(Account *account, QObject *parent = 0);

    enum Status {
        Undefined,
        Connected,
        NotConfigured,
        ServerVersionMismatch,
        CredentialsTooManyAttempts,
        CredentialError,
        CredentialsUserCanceled,
        CredentialsWrong,
        StatusNotFound

    };

    QStringList errors() const;
    bool networkError() const;

    void checkConnection();

    QString statusString( Status ) const;

signals:
    void connectionResult( ConnectionValidator::Status );
    // void connectionAvailable();
    // void connectionFailed();

public slots:

protected slots:
    void slotStatusFound(const QUrl&url, const QVariantMap &info);
    void slotNoStatusFound(QNetworkReply *reply);

    void slotCheckAuthentication();
    void slotAuthFailed(QNetworkReply *reply);
    void slotAuthSuccess();

private:
    QStringList _errors;
    Account   *_account;
    bool  _networkError;
};

}

#endif // CONNECTIONVALIDATOR_H
