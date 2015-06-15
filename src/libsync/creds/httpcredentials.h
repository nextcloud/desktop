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

#include "creds/abstractcredentials.h"

class QNetworkReply;
class QAuthenticator;

namespace QKeychain {
class Job;
}

namespace OCC
{

class OWNCLOUDSYNC_EXPORT HttpCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    explicit HttpCredentials();
    HttpCredentials(const QString& user, const QString& password, const QString& certificatePath,  const QString& certificatePasswd);

    void syncContextPreInit(CSYNC* ctx) Q_DECL_OVERRIDE;
    void syncContextPreStart(CSYNC* ctx) Q_DECL_OVERRIDE;
    bool changed(AbstractCredentials* credentials) const Q_DECL_OVERRIDE;
    QString authType() const Q_DECL_OVERRIDE;
    QNetworkAccessManager* getQNAM() const Q_DECL_OVERRIDE;
    bool ready() const Q_DECL_OVERRIDE;
    void fetch() Q_DECL_OVERRIDE;
    bool stillValid(QNetworkReply *reply) Q_DECL_OVERRIDE;
    void persist() Q_DECL_OVERRIDE;
    QString user() const Q_DECL_OVERRIDE;
    QString password() const;
    virtual QString queryPassword(bool *ok) = 0;
    void invalidateToken() Q_DECL_OVERRIDE;
    QString fetchUser();
    virtual bool sslIsTrusted() { return false; }
    QString certificatePath() const;
    QString certificatePasswd() const;

    // To fetch the user name as early as possible
    void setAccount(Account* account) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void slotAuthentication(QNetworkReply*, QAuthenticator*);
    void slotReadJobDone(QKeychain::Job*);
    void slotWriteJobDone(QKeychain::Job*);

protected:
    QString _user;
    QString _password;
    QString _previousPassword;

private:
    QString _certificatePath;
    QString _certificatePasswd;
    bool _ready;
    bool _fetchJobInProgress; //True if the keychain job is in progress or the input dialog visible
    bool _readPwdFromDeprecatedPlace;
};

} // namespace OCC

#endif
