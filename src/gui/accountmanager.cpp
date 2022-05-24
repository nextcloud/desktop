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
#include "configfile.h"
#include "creds/credentialmanager.h"
#include "proxyauthhandler.h"
#include "common/asserts.h"
#include <theme.h>
#include <creds/httpcredentialsgui.h>
#include <cookiejar.h>
#include <QSettings>
#include <QDir>
#include <QNetworkAccessManager>

namespace {
const char urlC[] = "url";
const char userC[] = "user";
const char httpUserC[] = "http_user";
const auto defaultSyncRootC()
{
    return QStringLiteral("default_sync_root");
}

const QString davUserC()
{
    return QStringLiteral("dav_user");
}

const QString davUserDisplyNameC()
{
    return QStringLiteral("display-name");
}

const QString userUUIDC()
{
    return QStringLiteral("uuid");
}

static const char caCertsKeyC[] = "CaCertificates";
static const char accountsC[] = "Accounts";
static const char versionC[] = "version";
static const char serverVersionC[] = "serverVersion";

auto capabilitesC()
{
    return QStringLiteral("capabilities");
}

// The maximum versions that this client can read
static const int maxAccountsVersion = 2;
static const int maxAccountVersion = 1;
}


namespace OCC {

Q_LOGGING_CATEGORY(lcAccountManager, "gui.account.manager", QtInfoMsg)

AccountManager *AccountManager::instance()
{
    static AccountManager instance;
    return &instance;
}

bool AccountManager::restore()
{
    QStringList skipSettingsKeys;
    backwardMigrationSettingsKeys(&skipSettingsKeys, &skipSettingsKeys);

    auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    if (settings->status() != QSettings::NoError) {
        qCWarning(lcAccountManager) << "Could not read settings from" << settings->fileName()
                                    << settings->status();
        return false;
    }

    if (skipSettingsKeys.contains(settings->group())) {
        // Should not happen: bad container keys should have been deleted
        qCWarning(lcAccountManager) << "Accounts structure is too new, ignoring";
        return true;
    }

    // If there are no accounts, check the old format.
    const auto &childGroups = settings->childGroups();
    if (childGroups.isEmpty()
        && !settings->contains(QLatin1String(versionC))) {
        restoreFromLegacySettings();
        return true;
    }

    for (const auto &accountId : childGroups) {
        settings->beginGroup(accountId);
        if (!skipSettingsKeys.contains(settings->group())) {
            if (auto acc = loadAccountHelper(*settings)) {
                acc->_id = accountId;
                if (auto accState = AccountState::loadFromSettings(acc, *settings)) {
                    addAccountState(accState);
                }
            }
        } else {
            qCInfo(lcAccountManager) << "Account" << accountId << "is too new, ignoring";
            _additionalBlockedAccountIds.insert(accountId);
        }
        settings->endGroup();
    }

    return true;
}

void AccountManager::backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    const int accountsVersion = settings->value(QLatin1String(versionC)).toInt();
    if (accountsVersion <= maxAccountsVersion) {
        const auto &childGroups = settings->childGroups();
        for (const auto &accountId : childGroups) {
            settings->beginGroup(accountId);
            const int accountVersion = settings->value(QLatin1String(versionC), 1).toInt();
            if (accountVersion > maxAccountVersion) {
                ignoreKeys->append(settings->group());
            }
            settings->endGroup();
        }
    } else {
        deleteKeys->append(settings->group());
    }
}

bool AccountManager::restoreFromLegacySettings()
{
    static const auto logPrefix = QStringLiteral("Legacy settings migration: ");

    qCInfo(lcAccountManager) << logPrefix
                             << "restoreFromLegacySettings, checking settings group"
                             << Theme::instance()->appName();

    // try to open the correctly themed settings
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());

    // if the settings file could not be opened, the childKeys list is empty
    // then try to load settings from a very old place
    if (settings->childKeys().isEmpty()) {
        // Now try to open the original ownCloud settings to see if they exist.
        QString oCCfgFile = QDir::fromNativeSeparators(settings->fileName());
        // replace the last two segments with ownCloud/owncloud.cfg
        oCCfgFile = oCCfgFile.left(oCCfgFile.lastIndexOf('/'));
        oCCfgFile = oCCfgFile.left(oCCfgFile.lastIndexOf('/'));
        oCCfgFile += QLatin1String("/ownCloud/owncloud.cfg");

        qCInfo(lcAccountManager) << logPrefix
                                 << "checking old config " << oCCfgFile;

#ifdef Q_OS_WIN
        Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
        QFileInfo fi(oCCfgFile);
        if (fi.isReadable()) {
            auto oCSettings = std::make_unique<QSettings>(oCCfgFile, QSettings::IniFormat);
            oCSettings->beginGroup(QLatin1String("ownCloud"));

            // Check the theme url to see if it is the same url that the oC config was for
            QString overrideUrl = Theme::instance()->overrideServerUrlV2();
            if (!overrideUrl.isEmpty()) {
                if (overrideUrl.endsWith('/')) {
                    overrideUrl.chop(1);
                }
                QString oCUrl = oCSettings->value(QLatin1String(urlC)).toString();
                if (oCUrl.endsWith('/')) {
                    oCUrl.chop(1);
                }

                // in case the urls are equal reset the settings object to read from
                // the ownCloud settings object
                qCInfo(lcAccountManager) << logPrefix
                                         << "Migrate oC config if " << oCUrl << " == " << overrideUrl << ":"
                                         << (oCUrl == overrideUrl ? "Yes" : "No");
                if (oCUrl == overrideUrl) {
                    settings = std::move(oCSettings);
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
    auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->setValue(QLatin1String(versionC), maxAccountsVersion);
    for (const auto &acc : qAsConst(_accounts)) {
        settings->beginGroup(acc->account()->id());
        saveAccountHelper(acc->account().data(), *settings, saveCredentials);
        acc->writeToSettings(*settings);
        settings->endGroup();
    }

    settings->sync();
    qCInfo(lcAccountManager) << "Saved all account settings, status:" << settings->status();
}

void AccountManager::saveAccount(Account *a)
{
    qCDebug(lcAccountManager) << "Saving account" << a->url().toString();
    auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->beginGroup(a->id());
    saveAccountHelper(a, *settings, false); // don't save credentials they might not have been loaded yet
    settings->endGroup();

    settings->sync();
    qCDebug(lcAccountManager) << "Saved account settings, status:" << settings->status();
}

QStringList AccountManager::accountNames() const
{
    QStringList accounts;
    accounts.reserve(AccountManager::instance()->accounts().size());
    for (const auto &a : AccountManager::instance()->accounts()) {
        accounts << a->account()->displayName();
    }
    std::sort(accounts.begin(), accounts.end());
    return accounts;
}

void AccountManager::saveAccountHelper(Account *acc, QSettings &settings, bool saveCredentials)
{
    settings.setValue(QLatin1String(versionC), maxAccountVersion);
    settings.setValue(QLatin1String(urlC), acc->_url.toString());
    settings.setValue(davUserC(), acc->_davUser);
    settings.setValue(davUserDisplyNameC(), acc->_displayName);
    settings.setValue(userUUIDC(), acc->uuid());
    if (acc->hasCapabilities()) {
        settings.setValue(capabilitesC(), acc->capabilities().raw());
    }
    if (acc->hasDefaultSyncRoot()) {
        settings.setValue(defaultSyncRootC(), acc->defaultSyncRoot());
    }
    if (acc->_credentials) {
        if (saveCredentials) {
            // Only persist the credentials if the parameter is set, on migration from 1.8.x
            // we want to save the accounts but not overwrite the credentials
            // (This is easier than asynchronously fetching the credentials from keychain and then
            // re-persisting them)
            acc->_credentials->persist();
        }

        for (auto it = acc->_settingsMap.constBegin(); it != acc->_settingsMap.constEnd(); ++it) {
            settings.setValue(it.key(), it.value());
        }

        // HACK: Save http_user also as user
        if (acc->_settingsMap.contains(httpUserC))
            settings.setValue(userC, acc->_settingsMap.value(httpUserC));
    }

    // Save accepted certificates.
    settings.beginGroup(QLatin1String("General"));
    qCInfo(lcAccountManager) << "Saving " << acc->approvedCerts().count() << " unknown certs.";
    const auto approvedCerts = acc->approvedCerts();
    QByteArray certs;
    for (const auto &cert : approvedCerts) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings.setValue(QLatin1String(caCertsKeyC), certs);
    }
    settings.endGroup();
}

AccountPtr AccountManager::loadAccountHelper(QSettings &settings)
{
    auto urlConfig = settings.value(QLatin1String(urlC));
    if (!urlConfig.isValid()) {
        // No URL probably means a corrupted entry in the account settings
        qCWarning(lcAccountManager) << "No URL for account " << settings.group();
        return AccountPtr();
    }

    auto acc = createAccount();

    QString overrideUrl = Theme::instance()->overrideServerUrlV2();
    QString forceAuth = Theme::instance()->forceConfigAuthType();
    if (!forceAuth.isEmpty() && !overrideUrl.isEmpty()) {
        // If forceAuth is set, this might also mean the overrideURL has changed.
        // See enterprise issues #1126
        acc->setUrl(overrideUrl);
    } else {
        acc->setUrl(urlConfig.toUrl());
    }

    acc->_davUser = settings.value(davUserC()).toString();
    acc->_displayName = settings.value(davUserDisplyNameC()).toString();
    acc->_uuid = settings.value(userUUIDC(), acc->_uuid).toUuid();
    acc->setCapabilities(settings.value(capabilitesC()).value<QVariantMap>());
    acc->setDefaultSyncRoot(settings.value(defaultSyncRootC()).toString());

    // We want to only restore settings for that auth type and the user value
    acc->_settingsMap.insert(QLatin1String(userC), settings.value(userC));
    const QString authTypePrefix = QStringLiteral("http_");
    const auto childKeys = settings.childKeys();
    for (const auto &key : childKeys) {
        if (!key.startsWith(authTypePrefix))
            continue;
        acc->_settingsMap.insert(key, settings.value(key));
    }
    acc->setCredentials(new HttpCredentialsGui);

    // now the server cert, it is in the general group
    settings.beginGroup(QLatin1String("General"));
    const auto certs = QSslCertificate::fromData(settings.value(caCertsKeyC).toByteArray());
    qCInfo(lcAccountManager) << "Restored: " << certs.count() << " unknown certs.";
    acc->setApprovedCerts(certs);
    settings.endGroup();

    return acc;
}

AccountStatePtr AccountManager::account(const QString &name)
{
    for (const auto &acc : qAsConst(_accounts)) {
        if (acc->account()->displayName() == name) {
            return acc;
        }
    }
    return AccountStatePtr();
}

AccountStatePtr AccountManager::account(const QUuid uuid) {
    return _accounts.value(uuid);
}

AccountStatePtr AccountManager::addAccount(const AccountPtr &newAccount)
{
    auto id = newAccount->id();
    if (id.isEmpty() || !isAccountIdAvailable(id)) {
        id = generateFreeAccountId();
    }
    newAccount->_id = id;

    AccountStatePtr newAccountState(AccountState::fromNewAccount(newAccount));
    addAccountState(newAccountState);
    return newAccountState;
}

void AccountManager::deleteAccount(AccountStatePtr account)
{
    auto it = std::find(_accounts.begin(), _accounts.end(), account);
    if (it == _accounts.end()) {
        return;
    }
    // The argument keeps a strong reference to the AccountState, so we can safely remove other
    // AccountStatePtr occurrences:
    _accounts.erase(it);

    // Forget account credentials, cookies
    account->account()->credentials()->forgetSensitiveData();
    account->account()->credentialManager()->clear();

    auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->remove(account->account()->id());

    emit accountRemoved(account);
}

AccountPtr AccountManager::createAccount()
{
    AccountPtr acc = Account::create();
    connect(acc.data(), &Account::proxyAuthenticationRequired,
        ProxyAuthHandler::instance(), &ProxyAuthHandler::handleProxyAuthenticationRequired);
    return acc;
}

void AccountManager::shutdown()
{
    const auto accounts = std::move(_accounts);
    for (const auto &acc : accounts) {
        emit accountRemoved(acc);
    }
}

bool AccountManager::isAccountIdAvailable(const QString &id) const
{
    for (const auto &acc : _accounts) {
        if (acc->account()->id() == id) {
            return false;
        }
    }
    if (_additionalBlockedAccountIds.contains(id))
        return false;
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

void AccountManager::addAccountState(AccountStatePtr accountState)
{
    QObject::connect(accountState->account().data(),
        &Account::wantsAccountSaved,
        this, &AccountManager::saveAccount);

    _accounts.insert(accountState->account()->uuid(), accountState);
    emit accountAdded(accountState);
}
}
