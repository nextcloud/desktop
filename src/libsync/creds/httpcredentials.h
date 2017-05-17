/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_HTTP_CREDENTIALS_H
#define MIRALL_CREDS_HTTP_CREDENTIALS_H

#include <QMap>
#include <QSslCertificate>
#include <QSslKey>
#include "creds/abstractcredentials.h"

class QNetworkReply;
class QAuthenticator;

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

class OWNCLOUDSYNC_EXPORT HttpCredentials : public AbstractCredentials
{
    Q_OBJECT
    friend class HttpCredentialsAccessManager;

public:
    explicit HttpCredentials();
    HttpCredentials(const QString &user, const QString &password, const QSslCertificate &certificate = QSslCertificate(), const QSslKey &key = QSslKey());

    QString authType() const Q_DECL_OVERRIDE;
    QNetworkAccessManager *getQNAM() const Q_DECL_OVERRIDE;
    bool ready() const Q_DECL_OVERRIDE;
    void fetchFromKeychain() Q_DECL_OVERRIDE;
    bool stillValid(QNetworkReply *reply) Q_DECL_OVERRIDE;
    void persist() Q_DECL_OVERRIDE;
    QString user() const Q_DECL_OVERRIDE;
    QString password() const;
    void invalidateToken() Q_DECL_OVERRIDE;
    void forgetSensitiveData() Q_DECL_OVERRIDE;
    QString fetchUser();
    virtual bool sslIsTrusted() { return false; }

    // To fetch the user name as early as possible
    void setAccount(Account *account) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void slotAuthentication(QNetworkReply *, QAuthenticator *);

    void slotReadClientCertPEMJobDone(QKeychain::Job *);
    void slotReadClientKeyPEMJobDone(QKeychain::Job *);
    void slotReadJobDone(QKeychain::Job *);

    void slotWriteClientCertPEMJobDone(QKeychain::Job *);
    void slotWriteClientKeyPEMJobDone(QKeychain::Job *);
    void slotWriteJobDone(QKeychain::Job *);
    void clearQNAMCache();

protected:
    QString _user;
    QString _password;
    QString _previousPassword;

    QString _fetchErrorString;
    bool _ready;
    QSslKey _clientSslKey;
    QSslCertificate _clientSslCertificate;
};

} // namespace OCC

#endif
