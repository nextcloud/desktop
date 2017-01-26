/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "accountmanager.h"
#include "sslerrordialog.h"
#include "proxyauthhandler.h"
#include <theme.h>
#include <creds/credentialsfactory.h>
#include <creds/abstractcredentials.h>
#include <cookiejar.h>
#include <QSettings>
#include <QDir>
#include <QNetworkAccessManager>

namespace {
static const char urlC[] = "url";
static const char authTypeC[] = "authType";
static const char userC[] = "user";
static const char httpUserC[] = "http_user";
static const char caCertsKeyC[] = "CaCertificates";
static const char accountsC[] = "Accounts";
static const char versionC[] = "version";
static const char serverVersionC[] = "serverVersion";
}


namespace OCC {

AccountManager *AccountManager::instance()
{
    static AccountManager instance;
    return &instance;
}

bool AccountManager::restore()
{
    auto settings = Utility::settingsWithGroup(QLatin1String(accountsC));

    // If there are no accounts, check the old format.
    if (settings->childGroups().isEmpty()
            && !settings->contains(QLatin1String(versionC))) {
        return restoreFromLegacySettings();
    }

    foreach (const auto& accountId, settings->childGroups()) {
        settings->beginGroup(accountId);
        if (auto acc = loadAccountHelper(*settings)) {
            acc->_id = accountId;
            if (auto accState = AccountState::loadFromSettings(acc, *settings)) {
                addAccountState(accState);
            }
        }
        settings->endGroup();
    }

    return true;
}

bool AccountManager::restoreFromLegacySettings()
{
    // try to open the correctly themed settings
    auto settings = Utility::settingsWithGroup(Theme::instance()->appName());

    // if the settings file could not be opened, the childKeys list is empty
    // then try to load settings from a very old place
    if( settings->childKeys().isEmpty() ) {
        // Now try to open the original ownCloud settings to see if they exist.
        QString oCCfgFile = QDir::fromNativeSeparators( settings->fileName() );
        // replace the last two segments with ownCloud/owncloud.cfg
        oCCfgFile = oCCfgFile.left( oCCfgFile.lastIndexOf('/'));
        oCCfgFile = oCCfgFile.left( oCCfgFile.lastIndexOf('/'));
        oCCfgFile += QLatin1String("/ownCloud/owncloud.cfg");

        qDebug() << "Migrate: checking old config " << oCCfgFile;

        QFileInfo fi( oCCfgFile );
        if( fi.isReadable() ) {
            QSettings *oCSettings = new QSettings(oCCfgFile, QSettings::IniFormat);
            oCSettings->beginGroup(QLatin1String("ownCloud"));

            // Check the theme url to see if it is the same url that the oC config was for
            QString overrideUrl = Theme::instance()->overrideServerUrl();
            if( !overrideUrl.isEmpty() ) {
                if (overrideUrl.endsWith('/')) { overrideUrl.chop(1); }
                QString oCUrl = oCSettings->value(QLatin1String(urlC)).toString();
                if (oCUrl.endsWith('/')) { oCUrl.chop(1); }

                // in case the urls are equal reset the settings object to read from
                // the ownCloud settings object
                qDebug() << "Migrate oC config if " << oCUrl << " == " << overrideUrl << ":"
                         << (oCUrl == overrideUrl ? "Yes" : "No");
                if( oCUrl == overrideUrl ) {
                    settings.reset( oCSettings );
                } else {
                    delete oCSettings;
                }
            }
        }
    }

    // Try to load the single account.
    if (!settings->childKeys().isEmpty()) {
        if (auto acc = loadAccountHelper(*settings)) {
            addAccount(acc);
            return true;
        }
    }
    return false;
}

void AccountManager::save(bool saveCredentials)
{
    auto settings = Utility::settingsWithGroup(QLatin1String(accountsC));
    settings->setValue(QLatin1String(versionC), 2);
    foreach (const auto &acc, _accounts) {
        settings->beginGroup(acc->account()->id());
        saveAccountHelper(acc->account().data(), *settings, saveCredentials);
        acc->writeToSettings(*settings);
        settings->endGroup();
    }

    settings->sync();
    qDebug() << "Saved all account settings, status:" << settings->status();
}

void AccountManager::saveAccount(Account* a)
{
    qDebug() << "Saving account" << a->url().toString();
    auto settings = Utility::settingsWithGroup(QLatin1String(accountsC));
    settings->beginGroup(a->id());
    saveAccountHelper(a, *settings, false); // don't save credentials they might not have been loaded yet
    settings->endGroup();

    settings->sync();
    qDebug() << "Saved account settings, status:" << settings->status();
}

void AccountManager::saveAccountState(AccountState* a)
{
    qDebug() << "Saving account state" << a->account()->url().toString();
    auto settings = Utility::settingsWithGroup(QLatin1String(accountsC));
    settings->beginGroup(a->account()->id());
    a->writeToSettings(*settings);
    settings->endGroup();

    settings->sync();
    qDebug() << "Saved account state settings, status:" << settings->status();
}

void AccountManager::saveAccountHelper(Account* acc, QSettings& settings, bool saveCredentials)
{
    settings.setValue(QLatin1String(urlC), acc->_url.toString());
    settings.setValue(QLatin1String(serverVersionC), acc->_serverVersion);
    if (acc->_credentials) {
        if (saveCredentials) {
            // Only persist the credentials if the parameter is set, on migration from 1.8.x
            // we want to save the accounts but not overwrite the credentials
            // (This is easier than asynchronously fetching the credentials from keychain and then
            // re-persisting them)
            acc->_credentials->persist();
        }
        Q_FOREACH(QString key, acc->_settingsMap.keys()) {
            settings.setValue(key, acc->_settingsMap.value(key));
        }
        settings.setValue(QLatin1String(authTypeC), acc->_credentials->authType());

        // HACK: Save http_user also as user
        if (acc->_settingsMap.contains(httpUserC))
            settings.setValue(userC, acc->_settingsMap.value(httpUserC));
    }

    // Save accepted certificates.
    settings.beginGroup(QLatin1String("General"));
    qDebug() << "Saving " << acc->approvedCerts().count() << " unknown certs.";
    QByteArray certs;
    Q_FOREACH( const QSslCertificate& cert, acc->approvedCerts() ) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings.setValue( QLatin1String(caCertsKeyC), certs );
    }
    settings.endGroup();

    // Save cookies.
    if (acc->_am) {
        CookieJar* jar = qobject_cast<CookieJar*>(acc->_am->cookieJar());
        if (jar) {
            qDebug() << "Saving cookies." << acc->cookieJarPath();
            jar->save(acc->cookieJarPath());
        }
    }
}

AccountPtr AccountManager::loadAccountHelper(QSettings& settings)
{
    auto urlConfig = settings.value(QLatin1String(urlC));
    if (!urlConfig.isValid()) {
        // No URL probably means a corrupted entry in the account settings
        qDebug() << "No URL for account " << settings.group();
        return AccountPtr();
    }

    auto acc = createAccount();

    QString authType = settings.value(QLatin1String(authTypeC)).toString();

    // There was an account-type saving bug when 'skip folder config' was used
    // See #5408. This attempts to fix up the "dummy" authType
    if (authType == QLatin1String("dummy")) {
        if (settings.contains(QLatin1String("http_user"))) {
            authType = "http";
        } else if (settings.contains(QLatin1String("shibboleth_shib_user"))) {
            authType = "shibboleth";
        }
    }

    QString overrideUrl = Theme::instance()->overrideServerUrl();
    QString forceAuth = Theme::instance()->forceConfigAuthType();
    if(!forceAuth.isEmpty() && !overrideUrl.isEmpty() ) {
        // If forceAuth is set, this might also mean the overrideURL has changed.
        // See enterprise issues #1126
        acc->setUrl(overrideUrl);
        authType = forceAuth;
    } else {
        acc->setUrl(urlConfig.toUrl());
    }

    qDebug() << "Account for" << acc->url() << "using auth type" << authType;

    acc->_serverVersion = settings.value(QLatin1String(serverVersionC)).toString();

    // We want to only restore settings for that auth type and the user value
    acc->_settingsMap.insert(QLatin1String(userC), settings.value(userC));
    QString authTypePrefix = authType + "_";
    Q_FOREACH(QString key, settings.childKeys()) {
        if (!key.startsWith(authTypePrefix))
            continue;
        acc->_settingsMap.insert(key, settings.value(key));
    }

    acc->setCredentials(CredentialsFactory::create(authType));

    // now the server cert, it is in the general group
    settings.beginGroup(QLatin1String("General"));
    acc->setApprovedCerts(QSslCertificate::fromData(settings.value(caCertsKeyC).toByteArray()));
    settings.endGroup();

    return acc;
}

AccountStatePtr AccountManager::account(const QString& name)
{
    foreach (const auto& acc, _accounts) {
        if (acc->account()->displayName() == name) {
            return acc;
        }
    }
    return AccountStatePtr();
}

AccountState *AccountManager::addAccount(const AccountPtr& newAccount)
{
    auto id = newAccount->id();
    if (id.isEmpty() || !isAccountIdAvailable(id)) {
        id = generateFreeAccountId();
    }
    newAccount->_id = id;

    auto newAccountState = new AccountState(newAccount);
    addAccountState(newAccountState);
    return newAccountState;
}

void AccountManager::deleteAccount(AccountState* account)
{
    auto it = std::find(_accounts.begin(), _accounts.end(), account);
    if (it == _accounts.end()) { return; }
    auto copy = *it; // keep a reference to the shared pointer so it does not delete it just yet
    _accounts.erase(it);

    QFile::remove(account->account()->cookieJarPath());

    auto settings = Utility::settingsWithGroup(QLatin1String(accountsC));
    settings->remove(account->account()->id());

    emit accountRemoved(account);
}

AccountPtr AccountManager::createAccount()
{
    AccountPtr acc = Account::create();
    acc->setSslErrorHandler(new SslDialogErrorHandler);
    connect(acc.data(), SIGNAL(proxyAuthenticationRequired(QNetworkProxy, QAuthenticator*)),
            ProxyAuthHandler::instance(), SLOT(handleProxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)));
    return acc;
}


void AccountManager::shutdown()
{
    auto accountsCopy = _accounts;
    _accounts.clear();
    foreach (const auto &acc, accountsCopy) {
        emit accountRemoved(acc.data());
    }
}

bool AccountManager::isAccountIdAvailable(const QString& id) const
{
    foreach (const auto& acc, _accounts) {
        if (acc->account()->id() == id) {
            return false;
        }
    }
    return true;
}

QString AccountManager::generateFreeAccountId() const
{
    int i = 0;
    forever {
        QString id = QString::number(i);
        if (isAccountIdAvailable(id)) {
            return id;
        }
        ++i;
    }
}

void AccountManager::addAccountState(AccountState* accountState)
{
    QObject::connect(accountState->account().data(),
                     SIGNAL(wantsAccountSaved(Account*)),
                     SLOT(saveAccount(Account*)));

    AccountStatePtr ptr(accountState);
    _accounts << ptr;
    emit accountAdded(accountState);
}

}
