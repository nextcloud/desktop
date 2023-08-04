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

#include "creds/abstractcredentials.h"
#include "creds/oauth.h"
#include "networkjobs.h"

#include <QMap>
#include <QSslCertificate>
#include <QSslKey>
#include <QNetworkRequest>

class QNetworkReply;
class QAuthenticator;

namespace OCC {
class OAuth;

/*
   The authentication system is this way because of Shibboleth.
   There used to be two different ways to authenticate: Shibboleth and HTTP Basic Auth.
   AbstractCredentials can be inherited from both ShibbolethCrendentials and HttpCredentials.

   HttpCredentials is then split in HttpCredentials and HttpCredentialsGui.

   This class handle both HTTP Basic Auth and OAuth. But anything that needs GUI to ask the user
   is in HttpCredentialsGui.

 */
class OWNCLOUDSYNC_EXPORT HttpCredentials : public AbstractCredentials
{
    Q_OBJECT
    friend class HttpCredentialsAccessManager;

public:
    /// Don't add credentials if this is set on a QNetworkRequest
    static constexpr QNetworkRequest::Attribute DontAddCredentialsAttribute = QNetworkRequest::User;

    explicit HttpCredentials(DetermineAuthTypeJob::AuthType authType, const QString &user, const QString &password);

    QString authType() const override;
    AccessManager *createAM() const override;
    bool ready() const override;
    void fetchFromKeychain() override;
    bool stillValid(QNetworkReply *reply) override;
    void persist() override;
    QString user() const override;
    void invalidateToken() override;
    void forgetSensitiveData() override;
    QString fetchUser();

    /* If we still have a valid refresh token, try to refresh it assynchronously and emit fetched()
     * otherwise return false
     */
    bool refreshAccessToken();

    // To fetch the user name as early as possible
    void setAccount(Account *account) override;

    // Whether we are using OAuth
    bool isUsingOAuth() const { return _authType == DetermineAuthTypeJob::AuthType::OAuth; }
protected:
    HttpCredentials() = default;

    void slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator);
    void fetchFromKeychainHelper();

    QString _user;
    QString _password; // user's password, or access_token for OAuth
    QString _refreshToken; // OAuth _refreshToken, set if OAuth is used.
    QString _previousPassword;

    QString _fetchErrorString;
    bool _ready = false;
    QPointer<AccountBasedOAuth> _oAuthJob;

    DetermineAuthTypeJob::AuthType _authType = DetermineAuthTypeJob::AuthType::Unknown;

private:
    bool refreshAccessTokenInternal(int tokenRefreshRetriesCount);
};


} // namespace OCC

#endif
