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

namespace OCC
{

class TokenCredentialsAccessManager;
class OWNCLOUDSYNC_EXPORT TokenCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    friend class TokenCredentialsAccessManager;
    TokenCredentials();
    TokenCredentials(const QString& user, const QString& password, const QString &token);

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
    void invalidateToken() Q_DECL_OVERRIDE;

    QString password() const;
private Q_SLOTS:
    void slotAuthentication(QNetworkReply*, QAuthenticator*);

private:
    QString _user;
    QString _password;
    QString _token; // the cookies
    bool _ready;
};

} // namespace OCC

#endif
