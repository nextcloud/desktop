/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#pragma once

#include "account.h"
#include "gui/creds/httpcredentialsgui.h"
#include "networkjobs.h"

namespace OCC::Wizard {

/**
 * The server can use varying authentication methods, for instance HTTP Basic or OAuth2.
 * Depending on the concrete authentication method the server uses, the account's credentials must be initialized differently.
 * We use the strategy pattern to be able to model multiple methods and allow adding new ones by just adding another strategy implementation.
 */
class AbstractAuthenticationStrategy
{
public:
    virtual ~AbstractAuthenticationStrategy();

    /**
     * Create credentials object for use in the account.
     * @return credentials
     */
    virtual HttpCredentialsGui *makeCreds() = 0;

    /**
     * Checks whether the passed credentials are valid.
     * @return true if valid, false otherwise
     */
    virtual bool isValid() = 0;

    /**
     * The username to use for WebDAV authentication along with the secret credentials.
     * For some reason, the credentials object must be seeded with this username separately.
     * @return username for use with WebDAV
     */
    virtual QString davUser() = 0;
};

class HttpBasicAuthenticationStrategy : public AbstractAuthenticationStrategy
{
public:
    explicit HttpBasicAuthenticationStrategy(const QString &username, const QString &password);

    HttpCredentialsGui *makeCreds() override;

    bool isValid() override;

    QString davUser() override;

    // access is needed to be able to check these credentials against the server
    QString username() const;
    QString password() const;

private:
    QString _username;
    QString _password;
};

class OAuth2AuthenticationStrategy : public AbstractAuthenticationStrategy
{
public:
    explicit OAuth2AuthenticationStrategy(const QString &davUser, const QString &token, const QString &refreshToken);

    HttpCredentialsGui *makeCreds() override;

    bool isValid() override;

    QString davUser() override;

private:
    QString _davUser;
    QString _token;
    QString _refreshToken;
};

/**
 * This class constructs an Account object from data entered by the user to the wizard resp. collected while checking the user's information.
 * The class does not perform any kind of validation. It is the caller's job to make sure the data is correct.
 */
class SetupWizardAccountBuilder
{
public:
    SetupWizardAccountBuilder();

    /**
     * Set server URL.
     * @param serverUrl URL to server
     */
    void setServerUrl(const QUrl &serverUrl, DetermineAuthTypeJob::AuthType workflowType);
    QUrl serverUrl() const;

    // TODO: move this out of the class's state
    DetermineAuthTypeJob::AuthType authType();

    void setAuthenticationStrategy(AbstractAuthenticationStrategy *strategy);
    AbstractAuthenticationStrategy *authenticationStrategy() const;

    /**
     * Check whether credentials passed to the builder so far can be used to create a new account object.
     * Note that this does not mean they are correct, the method only checks whether there is "enough" data.
     * @return true if credentials are valid, false otherwise
     */
    bool hasValidCredentials() const;

    QString displayName() const;

    /**
     * Store custom CA certificate for the newly built account.
     * @param customTrustedCaCertificate certificate to store
     */
    void addCustomTrustedCaCertificate(const QSslCertificate &customTrustedCaCertificate);

    /**
     * Remove all stored custom trusted CA certificates.
     */
    void clearCustomTrustedCaCertificates();

    /**
     * Attempt to build an account from the previously entered information.
     * @return built account or null if information is still missing
     */
    AccountPtr build();

private:
    QUrl _serverUrl;

    DetermineAuthTypeJob::AuthType _authType = DetermineAuthTypeJob::AuthType::Unknown;

    std::unique_ptr<AbstractAuthenticationStrategy> _authenticationStrategy;

    QSet<QSslCertificate> _customTrustedCaCertificates;
};
}
