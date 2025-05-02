/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    QNetworkAccessManager *createQNAM() const override;
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
