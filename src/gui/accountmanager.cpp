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
#include "account.h"
#include "configfile.h"
#include "creds/credentialmanager.h"
#include "guiutility.h"
#include <cookiejar.h>
#include <creds/httpcredentialsgui.h>
#include <theme.h>

#ifdef Q_OS_WIN
#include "common/utility_win.h"
#endif

#include <QSettings>
#include <QDir>
#include <QNetworkAccessManager>

namespace {
auto urlC()
{
    return QStringLiteral("url");
}

auto userC()
{
    return QStringLiteral("user");
}

auto httpUserC()
{
    return QStringLiteral("http_user");
}

auto defaultSyncRootC()
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

auto caCertsKeyC()
{
    return QStringLiteral("CaCertificates");
}

auto accountsC()
{
    return QStringLiteral("Accounts");
}

auto versionC()
{
    return QStringLiteral("version");
}

auto capabilitesC()
{
    return QStringLiteral("capabilities");
}
}


namespace OCC {

Q_LOGGING_CATEGORY(lcAccountManager, "gui.account.manager", QtInfoMsg)

AccountManager *AccountManager::instance()
{
    static AccountManager instance;
    return &instance;
}

AccountManager *AccountManager::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    Q_ASSERT(qmlEngine->thread() == AccountManager::instance()->thread());
    QJSEngine::setObjectOwnership(AccountManager::instance(), QJSEngine::CppOwnership);
    return instance();
}

bool AccountManager::restore()
{
    auto settings = ConfigFile::settingsWithGroup(accountsC());
    if (settings->status() != QSettings::NoError) {
        qCWarning(lcAccountManager) << "Could not read settings from" << settings->fileName()
                                    << settings->status();
        return false;
    }

    // If there are no accounts, check the old format.
    const auto &childGroups = settings->childGroups();
    if (childGroups.isEmpty()
        && !settings->contains(versionC())) {
        restoreFromLegacySettings();
        return true;
    }

    for (const auto &accountId : childGroups) {
        settings->beginGroup(accountId);
        if (auto acc = loadAccountHelper(*settings)) {
            acc->_id = accountId;
            if (auto accState = AccountState::loadFromSettings(acc, *settings)) {
                addAccountState(std::move(accState));
            }
        }
        settings->endGroup();
    }

    return true;
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
        oCCfgFile = oCCfgFile.left(oCCfgFile.lastIndexOf(QLatin1Char('/')));
        oCCfgFile = oCCfgFile.left(oCCfgFile.lastIndexOf(QLatin1Char('/')));
        oCCfgFile += QLatin1String("/ownCloud/owncloud.cfg");

        qCInfo(lcAccountManager) << logPrefix
                                 << "checking old config " << oCCfgFile;

#ifdef Q_OS_WIN
        Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
        QFileInfo fi(oCCfgFile);
        if (fi.isReadable()) {
            auto oCSettings = std::make_unique<QSettings>(oCCfgFile, QSettings::IniFormat);
            oCSettings->beginGroup(QStringLiteral("ownCloud"));

            // Check the theme url to see if it is the same url that the oC config was for
            QString overrideUrl = Theme::instance()->overrideServerUrlV2();
            if (!overrideUrl.isEmpty()) {
                if (overrideUrl.endsWith(QLatin1Char('/'))) {
                    overrideUrl.chop(1);
                }
                QString oCUrl = oCSettings->value(urlC()).toString();
                if (oCUrl.endsWith(QLatin1Char('/'))) {
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
    for (const auto &acc : qAsConst(_accounts)) {
        saveAccount(acc->account().data(), saveCredentials);
    }

    qCInfo(lcAccountManager) << "Saved all account settings";
}

void AccountManager::saveAccount(Account *account, bool saveCredentials)
{
    qCDebug(lcAccountManager) << "Saving account" << account->url().toString();
    auto settings = ConfigFile::settingsWithGroup(accountsC());
    settings->setValue(versionC(), ConfigFile::UnusedLegacySettingsVersionNumber);
    settings->beginGroup(account->id());

    settings->setValue(versionC(), ConfigFile::UnusedLegacySettingsVersionNumber);
    settings->setValue(urlC(), account->_url.toString());
    settings->setValue(davUserC(), account->_davUser);
    settings->setValue(davUserDisplyNameC(), account->_displayName);
    settings->setValue(userUUIDC(), account->uuid());
    if (account->hasCapabilities()) {
        settings->setValue(capabilitesC(), account->capabilities().raw());
    }
    if (account->hasDefaultSyncRoot()) {
        settings->setValue(defaultSyncRootC(), account->defaultSyncRoot());
    }
    if (account->_credentials) {
        if (saveCredentials) {
            // Only persist the credentials if the parameter is set, on migration from 1.8.x
            // we want to save the accounts but not overwrite the credentials
            // (This is easier than asynchronously fetching the credentials from keychain and then
            // re-persisting them)
            account->_credentials->persist();
        }

        for (auto it = account->_settingsMap.constBegin(); it != account->_settingsMap.constEnd(); ++it) {
            settings->setValue(it.key(), it.value());
        }

        // HACK: Save http_user also as user
        if (account->_settingsMap.contains(httpUserC()))
            settings->setValue(userC(), account->_settingsMap.value(httpUserC()));
    }

    // Save accepted certificates.
    settings->beginGroup(QStringLiteral("General"));
    qCInfo(lcAccountManager) << "Saving " << account->approvedCerts().count() << " unknown certs.";
    const auto approvedCerts = account->approvedCerts();
    QByteArray certs;
    for (const auto &cert : approvedCerts) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings->setValue(caCertsKeyC(), certs);
    }
    settings->endGroup();

    // save the account state
    this->account(account->uuid())->writeToSettings(*settings);
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

QList<AccountState *> AccountManager::accountsRaw() const
{
    QList<AccountState *> out;
    out.reserve(_accounts.size());
    for (auto &x : _accounts.values()) {
        out.append(x);
    }
    return out;
}

AccountPtr AccountManager::loadAccountHelper(QSettings &settings)
{
    auto urlConfig = settings.value(urlC());
    if (!urlConfig.isValid()) {
        // No URL probably means a corrupted entry in the account settings
        qCWarning(lcAccountManager) << "No URL for account " << settings.group();
        return AccountPtr();
    }

    auto acc = createAccount(settings.value(userUUIDC(), QVariant::fromValue(QUuid::createUuid())).toUuid());

    acc->setUrl(urlConfig.toUrl());

    acc->_davUser = settings.value(davUserC()).toString();
    acc->_displayName = settings.value(davUserDisplyNameC()).toString();
    acc->setCapabilities({acc->url(), settings.value(capabilitesC()).value<QVariantMap>()});
    acc->setDefaultSyncRoot(settings.value(defaultSyncRootC()).toString());

    // We want to only restore settings for that auth type and the user value
    acc->_settingsMap.insert(userC(), settings.value(userC()));
    const QString authTypePrefix = QStringLiteral("http_");
    const auto childKeys = settings.childKeys();
    for (const auto &key : childKeys) {
        if (!key.startsWith(authTypePrefix))
            continue;
        acc->_settingsMap.insert(key, settings.value(key));
    }
    acc->setCredentials(new HttpCredentialsGui);

    // now the server cert, it is in the general group
    settings.beginGroup(QStringLiteral("General"));
    const auto certs = QSslCertificate::fromData(settings.value(caCertsKeyC()).toByteArray());
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

    return addAccountState(AccountState::fromNewAccount(newAccount));
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

    if (account->account()->hasDefaultSyncRoot()) {
        Utility::unmarkDirectoryAsSyncRoot(account->account()->defaultSyncRoot());
    }

    // Forget account credentials, cookies
    account->account()->credentials()->forgetSensitiveData();
    account->account()->credentialManager()->clear();

    auto settings = ConfigFile::settingsWithGroup(accountsC());
    settings->remove(account->account()->id());

    Q_EMIT accountRemoved(account);
    Q_EMIT accountsChanged();
    account->deleteLater();
}

AccountPtr AccountManager::createAccount(const QUuid &uuid)
{
    AccountPtr acc = Account::create(uuid);
    return acc;
}

void AccountManager::shutdown()
{
    const auto accounts = std::move(_accounts);
    for (const auto &acc : accounts) {
        Q_EMIT accountRemoved(acc);
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
    while (true) {
        QString id = QString::number(i);
        if (isAccountIdAvailable(id)) {
            return id;
        }
        ++i;
    }
}

AccountStatePtr AccountManager::addAccountState(std::unique_ptr<AccountState> &&accountState)
{
    auto *rawAccount = accountState->account().data();
    connect(rawAccount, &Account::wantsAccountSaved, this, [rawAccount, this] {
        // persis the account, not the credentials, we don't know whether they are ready yet
        saveAccount(rawAccount, false);
    });

    AccountStatePtr statePtr = accountState.release();
    _accounts.insert(statePtr->account()->uuid(), statePtr);
    Q_EMIT accountAdded(statePtr);
    Q_EMIT accountsChanged();
    return statePtr;
}
}
