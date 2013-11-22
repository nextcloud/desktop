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

namespace Mirall
{

class HttpCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    HttpCredentials();
    HttpCredentials(const QString& user, const QString& password);

    void syncContextPreInit(CSYNC* ctx);
    void syncContextPreStart(CSYNC* ctx);
    bool changed(AbstractCredentials* credentials) const;
    QString authType() const;
    QNetworkAccessManager* getQNAM() const;
    bool ready() const;
    void fetch(Account *account);
    bool stillValid(QNetworkReply *reply);
    bool fetchFromUser(Account *account);
    void persist(Account *account);
    QString user() const;
    QString password() const;
    QString queryPassword(bool *ok);
    void invalidateToken(Account *account);

private Q_SLOTS:
    void slotAuthentication(QNetworkReply*, QAuthenticator*);
    void slotReadJobDone(QKeychain::Job*);
    void slotWriteJobDone(QKeychain::Job*);

private:
    static QString keychainKey(const QString &url, const QString &user);
    QString _user;
    QString _password;
    bool _ready;
};

} // ns Mirall

#endif
