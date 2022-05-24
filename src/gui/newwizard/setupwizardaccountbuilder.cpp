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

#include "setupwizardaccountbuilder.h"

#include "gui/accountmanager.h"

namespace OCC::Wizard {

AbstractAuthenticationStrategy::~AbstractAuthenticationStrategy() {};


HttpBasicAuthenticationStrategy::HttpBasicAuthenticationStrategy(const QString &username, const QString &password)
    : _username(username)
    , _password(password)
{
}

HttpCredentialsGui *HttpBasicAuthenticationStrategy::makeCreds()
{
    return new HttpCredentialsGui(_username, _password);
}

bool HttpBasicAuthenticationStrategy::isValid()
{
    return !_username.isEmpty() && !_password.isEmpty();
}

QString HttpBasicAuthenticationStrategy::davUser()
{
    return _username;
}

QString HttpBasicAuthenticationStrategy::username() const
{
    return _username;
}

QString HttpBasicAuthenticationStrategy::password() const
{
    return _password;
}

OAuth2AuthenticationStrategy::OAuth2AuthenticationStrategy(const QString &davUser, const QString &token, const QString &refreshToken)
    : _davUser(davUser)
    , _token(token)
    , _refreshToken(refreshToken)
{
}

HttpCredentialsGui *OAuth2AuthenticationStrategy::makeCreds()
{
    return new HttpCredentialsGui(_davUser, _token, _refreshToken);
}

bool OAuth2AuthenticationStrategy::isValid()
{
    return !_davUser.isEmpty() && !_token.isEmpty() && !_refreshToken.isEmpty();
}

QString OAuth2AuthenticationStrategy::davUser()
{
    return _davUser;
}

SetupWizardAccountBuilder::SetupWizardAccountBuilder() = default;

void SetupWizardAccountBuilder::setServerUrl(const QUrl &serverUrl, DetermineAuthTypeJob::AuthType authType)
{
    _serverUrl = serverUrl;
    _authType = authType;

    // to not keep credentials longer than necessary, we purge them whenever the URL is set
    // for this reason, we also don't insert already-known credentials on the credentials pages when switching to them
    _authenticationStrategy.reset();
}

QUrl SetupWizardAccountBuilder::serverUrl() const
{
    return _serverUrl;
}

DetermineAuthTypeJob::AuthType SetupWizardAccountBuilder::authType()
{
    return _authType;
}

AccountPtr SetupWizardAccountBuilder::build()
{
    auto newAccountPtr = Account::create();

    Q_ASSERT(!_serverUrl.isEmpty() && _serverUrl.isValid());
    newAccountPtr->setUrl(_serverUrl);

    Q_ASSERT(hasValidCredentials());

    // TODO: perhaps _authenticationStrategy->setUpAccountPtr(...) would be more elegant? no need for getters then
    newAccountPtr->setDavUser(_authenticationStrategy->davUser());
    newAccountPtr->setCredentials(_authenticationStrategy->makeCreds());

    newAccountPtr->setDavDisplayName(_displayName);

    newAccountPtr->addApprovedCerts({ _customTrustedCaCertificates.begin(), _customTrustedCaCertificates.end() });

    return newAccountPtr;
}

bool SetupWizardAccountBuilder::hasValidCredentials() const
{
    if (_authenticationStrategy == nullptr) {
        return false;
    }

    return _authenticationStrategy->isValid();
}

QString SetupWizardAccountBuilder::displayName() const
{
    return _displayName;
}

void SetupWizardAccountBuilder::setDisplayName(const QString &displayName)
{
    _displayName = displayName;
}

void SetupWizardAccountBuilder::setAuthenticationStrategy(AbstractAuthenticationStrategy *strategy)
{
    _authenticationStrategy.reset(strategy);
}

void SetupWizardAccountBuilder::addCustomTrustedCaCertificate(const QSslCertificate &customTrustedCaCertificate)
{
    _customTrustedCaCertificates.insert(customTrustedCaCertificate);
}

void SetupWizardAccountBuilder::clearCustomTrustedCaCertificates()
{
    _customTrustedCaCertificates.clear();
}

AbstractAuthenticationStrategy *SetupWizardAccountBuilder::authenticationStrategy() const
{
    return _authenticationStrategy.get();
}
}
