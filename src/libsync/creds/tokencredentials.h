/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (c) by Markus Goetz <guruz@owncloud.com>
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

namespace OCC {

class TokenCredentialsAccessManager;
class OWNCLOUDSYNC_EXPORT TokenCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    friend class TokenCredentialsAccessManager;
    TokenCredentials();
    TokenCredentials(const QString &user, const QString &password, const QString &token);

    QString authType() const override;
    AccessManager *createAM() const override;
    bool ready() const override;
    void askFromUser() override;
    void fetchFromKeychain() override;
    bool stillValid(QNetworkReply *reply) override;
    void persist() override;
    QString user() const override;
    void invalidateToken() override;
    void forgetSensitiveData() override;

    QString password() const;
private Q_SLOTS:
    void slotAuthentication(QNetworkReply *, QAuthenticator *);

private:
    QString _user;
    QString _password;
    QString _token; // the cookies
    bool _ready;
};

} // namespace OCC

#endif
