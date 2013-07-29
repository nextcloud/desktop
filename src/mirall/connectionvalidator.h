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

class QNetworkReply;

namespace Mirall {

class ConnectionValidator : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionValidator(QObject *parent = 0);
    explicit ConnectionValidator(const QString& connection, QObject *parent = 0);

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

    void checkConnection();

    QString statusString( Status ) const;

signals:
    void connectionResult( ConnectionValidator::Status );
    // void connectionAvailable();
    // void connectionFailed();

public slots:

protected slots:
    void slotStatusFound( const QString&, const QString&, const QString&, const QString& );
    void slotNoStatusFound(QNetworkReply *);

    void slotCheckAuthentication();
    void slotAuthCheck( const QString& ,QNetworkReply * );

private:
    QStringList _errors;
    QString     _connection;
};

}

#endif // CONNECTIONVALIDATOR_H
