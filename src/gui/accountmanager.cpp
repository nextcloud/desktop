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
#include "creds/credentialsfactory.h"
#include "creds/abstractcredentials.h"
#include "creds/keychainchunk.h"
#include "libsync/clientsideencryption.h"
#include "libsync/configfile.h"
#include "libsync/cookiejar.h"
#include "libsync/theme.h"

#include <QSettings>
#include <QDir>
#include <QNetworkAccessManager>
#include <QMessageBox>
#include <QPushButton>
#include <type_traits>

#include <qt6keychain/keychain.h>

namespace {
constexpr auto urlC = "url";
constexpr auto authTypeC = "authType";
constexpr auto userC = "user";
constexpr auto displayNameC = "displayName";
constexpr auto httpUserC = "http_user";
constexpr auto davUserC = "dav_user";
constexpr auto webflowUserC = "webflow_user";
constexpr auto shibbolethUserC = "shibboleth_shib_user";
constexpr auto caCertsKeyC = "CaCertificates";
constexpr auto accountsC = "Accounts";
constexpr auto versionC = "version";
constexpr auto serverVersionC = "serverVersion";
constexpr auto serverColorC = "serverColor";
constexpr auto serverTextColorC = "serverTextColor";
constexpr auto skipE2eeMetadataChecksumValidationC = "skipE2eeMetadataChecksumValidation";
constexpr auto networkProxySettingC = "networkProxySetting";
constexpr auto networkProxyTypeC = "networkProxyType";
constexpr auto networkProxyHostNameC = "networkProxyHostName";
constexpr auto networkProxyPortC = "networkProxyPort";
constexpr auto networkProxyNeedsAuthC = "networkProxyNeedsAuth";
constexpr auto networkProxyUserC = "networkProxyUser";
constexpr auto networkUploadLimitSettingC = "networkUploadLimitSetting";
constexpr auto networkDownloadLimitSettingC = "networkDownloadLimitSetting";
constexpr auto networkUploadLimitC = "networkUploadLimit";
constexpr auto networkDownloadLimitC = "networkDownloadLimit";
constexpr auto encryptionCertificateSha256FingerprintC = "encryptionCertificateSha256Fingerprint";
constexpr auto generalC = "General";

constexpr auto dummyAuthTypeC = "dummy";
constexpr auto httpAuthTypeC = "http";
constexpr auto webflowAuthTypeC = "webflow";
constexpr auto shibbolethAuthTypeC = "shibboleth";
constexpr auto httpAuthPrefix = "http_";
constexpr auto webflowAuthPrefix = "webflow_";

constexpr auto networkProxyPasswordKeychainKeySuffixC = "_proxy_password";

constexpr auto legacyRelativeConfigLocationC = "/ownCloud/owncloud.cfg";
constexpr auto legacyCfgFileNameC = "owncloud.cfg";

constexpr auto unbrandedRelativeConfigLocationC = "/Nextcloud/nextcloud.cfg";
constexpr auto unbrandedCfgFileNameC = "nextcloud.cfg";

// The maximum versions that this client can read
constexpr auto maxAccountsVersion = 2;
constexpr auto maxAccountVersion = 1;

constexpr auto serverHasValidSubscriptionC = "serverHasValidSubscription";
}


namespace OCC {

Q_LOGGING_CATEGORY(lcAccountManager, "nextcloud.gui.account.manager", QtInfoMsg)

AccountManager *AccountManager::instance()
{
    static AccountManager instance;
    return &instance;
}

AccountManager::AccountsRestoreResult AccountManager::restore(const bool alsoRestoreLegacySettings)
{
    QStringList skipSettingsKeys;
    backwardMigrationSettingsKeys(&skipSettingsKeys, &skipSettingsKeys);

    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    if (settings->status() != QSettings::NoError || !settings->isWritable()) {
        qCWarning(lcAccountManager) << "Could not read settings from" << settings->fileName()
                                    << settings->status();
        return AccountsRestoreFailure;
    }

    if (skipSettingsKeys.contains(settings->group())) {
        // Should not happen: bad container keys should have been deleted
        qCWarning(lcAccountManager) << "Accounts structure is too new, ignoring";
        return AccountsRestoreSuccessWithSkipped;
    }

    // If there are no accounts, check the old format.
    if (settings->childGroups().isEmpty() && !settings->contains(QLatin1String(versionC)) && alsoRestoreLegacySettings) {
        restoreFromLegacySettings();
        return AccountsRestoreSuccessFromLegacyVersion;
    }

    auto result = AccountsRestoreSuccess;
    const auto settingsChildGroups = settings->childGroups();
    for (const auto &accountId : settingsChildGroups) {
        settings->beginGroup(accountId);
        if (!skipSettingsKeys.contains(settings->group())) {
            if (const auto acc = loadAccountHelper(*settings)) {
                acc->_id = accountId;
                const auto accState = new AccountState(acc);
                const auto jar = qobject_cast<CookieJar*>(acc->_networkAccessManager->cookieJar());
                Q_ASSERT(jar);
                if (jar) {
                    jar->restore(acc->cookieJarPath());
                }
                addAccountState(accState);
            }
        } else {
            qCInfo(lcAccountManager) << "Account" << accountId << "is too new, ignoring";
            _additionalBlockedAccountIds.insert(accountId);
            result = AccountsRestoreSuccessWithSkipped;
        }
        settings->endGroup();
    }

    return result;
}

void AccountManager::backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    const auto accountsVersion = settings->value(QLatin1String(versionC)).toInt();

    if (accountsVersion <= maxAccountsVersion) {
        const auto settingsChildGroups = settings->childGroups();
        for (const auto &accountId : settingsChildGroups) {
            settings->beginGroup(accountId);
            const auto accountVersion = settings->value(QLatin1String(versionC), 1).toInt();

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
    qCInfo(lcAccountManager) << "Migrate: restoreFromLegacySettings, checking settings group"
                             << Theme::instance()->appName();

    // try to open the correctly themed settings
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());

    auto wasLegacyImportDialogDisplayed = false;
    const auto displayLegacyImportDialog = Theme::instance()->displayLegacyImportDialog();

    // if the settings file could not be opened, the childKeys list is empty
    // then try to load settings from a very old place
    if (settings->childKeys().isEmpty()) {
        // Legacy settings used QDesktopServices to get the location for the config folder in 2.4 and before
        const auto legacy2_4CfgSettingsLocation = QString(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/data"));
        const auto legacy2_4CfgFileParentFolder = legacy2_4CfgSettingsLocation.left(legacy2_4CfgSettingsLocation.lastIndexOf('/'));

        // 2.5+ (rest of 2.x series)
        const auto legacy2_5CfgSettingsLocation = QStandardPaths::writableLocation(Utility::isWindows() ? QStandardPaths::AppDataLocation : QStandardPaths::AppConfigLocation);
        const auto legacy2_5CfgFileParentFolder = legacy2_5CfgSettingsLocation.left(legacy2_5CfgSettingsLocation.lastIndexOf('/'));

        // Now try the locations we use today
        const auto fullLegacyCfgFile = QDir::fromNativeSeparators(settings->fileName());
        const auto legacyCfgFileParentFolder = fullLegacyCfgFile.left(fullLegacyCfgFile.lastIndexOf('/'));
        const auto legacyCfgFileGrandParentFolder = legacyCfgFileParentFolder.left(legacyCfgFileParentFolder.lastIndexOf('/'));

        const auto legacyCfgFileNamePath = QString(QStringLiteral("/") + legacyCfgFileNameC);
        const auto legacyCfgFileRelativePath = QString(legacyRelativeConfigLocationC);

        auto legacyLocations = QVector<QString>{legacy2_4CfgFileParentFolder + legacyCfgFileRelativePath,
                                                legacy2_5CfgFileParentFolder + legacyCfgFileRelativePath,
                                                legacyCfgFileParentFolder + legacyCfgFileNamePath,
                                                legacyCfgFileGrandParentFolder + legacyCfgFileRelativePath};

        if (Theme::instance()->isBranded()) {
            const auto unbrandedCfgFileNamePath = QString(QStringLiteral("/") + unbrandedCfgFileNameC);
            const auto unbrandedCfgFileRelativePath = QString(unbrandedRelativeConfigLocationC);
            legacyLocations.append({legacyCfgFileParentFolder + unbrandedCfgFileNamePath, legacyCfgFileGrandParentFolder + unbrandedCfgFileRelativePath});
        }

        for (const auto &configFile : legacyLocations) {
            auto oCSettings = std::make_unique<QSettings>(configFile, QSettings::IniFormat);
            if (oCSettings->status() != QSettings::Status::NoError) {
                qCInfo(lcAccountManager) << "Error reading legacy configuration file" << oCSettings->status();
                break;
            }

            oCSettings->beginGroup(QLatin1String(accountsC));
            const auto accountsListSize = oCSettings->childGroups().size();
            oCSettings->endGroup();
            if (const QFileInfo configFileInfo(configFile);
                configFileInfo.exists() && configFileInfo.isReadable()) {
                qCInfo(lcAccountManager) << "Migrate: checking old config " << configFile;
                if (!forceLegacyImport() && accountsListSize > 0 && displayLegacyImportDialog) {
                    wasLegacyImportDialogDisplayed = true;
                    const auto importQuestion = accountsListSize > 1
                        ? tr("%1 accounts were detected from a legacy desktop client.\n"
                             "Should the accounts be imported?").arg(QString::number(accountsListSize))
                        : tr("1 account was detected from a legacy desktop client.\n"
                             "Should the account be imported?");
                    const auto importMessageBox = new QMessageBox(QMessageBox::Question, tr("Legacy import"), importQuestion);
                    importMessageBox->addButton(tr("Import"), QMessageBox::AcceptRole);
                    const auto skipButton = importMessageBox->addButton(tr("Skip"), QMessageBox::DestructiveRole);
                    importMessageBox->exec();
                    if (importMessageBox->clickedButton() == skipButton) {
                        return false;
                    }
                }

                // Check the theme url to see if it is the same url that the oC config was for
                const auto overrideUrl = Theme::instance()->overrideServerUrl();
                const auto cleanOverrideUrl = overrideUrl.endsWith('/') ? overrideUrl.chopped(1) : overrideUrl;
                qCInfo(lcAccountManager) << "Migrate: overrideUrl" << cleanOverrideUrl;

                if (!cleanOverrideUrl.isEmpty()) {
                    oCSettings->beginGroup(QLatin1String(accountsC));
                    const auto accountsChildGroups = oCSettings->childGroups();
                    for (const auto &accountId : accountsChildGroups) {
                        oCSettings->beginGroup(accountId);
                        const auto oCUrl = oCSettings->value(QLatin1String(urlC)).toString();
                        const auto cleanOCUrl = oCUrl.endsWith('/') ? oCUrl.chopped(1) : oCUrl;

                        // in case the urls are equal reset the settings object to read from
                        // the ownCloud settings object
                        qCInfo(lcAccountManager) << "Migrate oC config if " << cleanOCUrl << " == " << cleanOverrideUrl << ":"
                                                 << (cleanOCUrl == cleanOverrideUrl ? "Yes" : "No");
                        if (cleanOCUrl == cleanOverrideUrl) {
                            qCInfo(lcAccountManager) << "Copy settings" << oCSettings->allKeys().join(", ");
                            oCSettings->endGroup(); // current accountID group
                            oCSettings->endGroup(); // accounts group
                            settings = std::move(oCSettings);
                            break;
                        }

                        oCSettings->endGroup();
                    }

                    if (oCSettings) {
                        oCSettings->endGroup();
                    }
                } else {
                    qCInfo(lcAccountManager) << "Copy settings" << oCSettings->allKeys().join(", ");
                    settings = std::move(oCSettings);
                }

                ConfigFile::setDiscoveredLegacyConfigPath(configFileInfo.canonicalPath());
                break;
            } else {
                qCInfo(lcAccountManager) << "Migrate: could not read old config " << configFile;
            }
        }
    }

    // Try to load the single account.
    if (!settings->childKeys().isEmpty()) {
        settings->beginGroup(accountsC);
        const auto childGroups = settings->childGroups();
        for (const auto &accountId : childGroups) {
            settings->beginGroup(accountId);
            if (const auto acc = loadAccountHelper(*settings)) {
                addAccount(acc);
            }
            settings->endGroup();
        }
        return true;
    }

    if (wasLegacyImportDialogDisplayed) {
        QMessageBox::information(nullptr,
                                 tr("Legacy import"),
                                 tr("Could not import accounts from legacy client configuration."));
    }

    return false;
}

void AccountManager::save(bool saveCredentials)
{
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->setValue(QLatin1String(versionC), maxAccountsVersion);
    for (const auto &acc : std::as_const(_accounts)) {
        settings->beginGroup(acc->account()->id());
        saveAccountHelper(acc->account().data(), *settings, saveCredentials);
        settings->endGroup();
    }

    settings->sync();
    qCInfo(lcAccountManager) << "Saved all account settings, status:" << settings->status();
}

void AccountManager::saveAccount(Account *newAccountData)
{
    qCDebug(lcAccountManager) << "Saving account" << newAccountData->url().toString();
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->beginGroup(newAccountData->id());
    saveAccountHelper(newAccountData, *settings, false); // don't save credentials they might not have been loaded yet
    settings->endGroup();

    settings->sync();
    qCDebug(lcAccountManager) << "Saved account settings, status:" << settings->status();
}

void AccountManager::saveAccountState(AccountState *a)
{
    qCDebug(lcAccountManager) << "Saving account state" << a->account()->url().toString();
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->beginGroup(a->account()->id());
    settings->endGroup();

    settings->sync();
    qCDebug(lcAccountManager) << "Saved account state settings, status:" << settings->status();
}

void AccountManager::saveAccountHelper(Account *account, QSettings &settings, bool saveCredentials)
{
    qCDebug(lcAccountManager) << "Saving settings to" << settings.fileName();
    settings.setValue(QLatin1String(versionC), maxAccountVersion);
    settings.setValue(QLatin1String(urlC), account->_url.toString());
    settings.setValue(QLatin1String(davUserC), account->_davUser);
    settings.setValue(QLatin1String(displayNameC), account->davDisplayName());
    settings.setValue(QLatin1String(serverVersionC), account->_serverVersion);
    settings.setValue(QLatin1String(serverColorC), account->_serverColor);
    settings.setValue(QLatin1String(serverTextColorC), account->_serverTextColor);
    settings.setValue(QLatin1String(serverHasValidSubscriptionC), account->serverHasValidSubscription());
    settings.setValue(QLatin1String(encryptionCertificateSha256FingerprintC), account->encryptionCertificateFingerprint());
    if (!account->_skipE2eeMetadataChecksumValidation) {
        settings.remove(QLatin1String(skipE2eeMetadataChecksumValidationC));
    } else {
        settings.setValue(QLatin1String(skipE2eeMetadataChecksumValidationC), account->_skipE2eeMetadataChecksumValidation);
    }
    settings.setValue(networkProxySettingC, static_cast<std::underlying_type_t<Account::AccountNetworkProxySetting>>(account->networkProxySetting()));
    settings.setValue(networkProxyTypeC, account->proxyType());
    settings.setValue(networkProxyHostNameC, account->proxyHostName());
    settings.setValue(networkProxyPortC, account->proxyPort());
    settings.setValue(networkProxyNeedsAuthC, account->proxyNeedsAuth());
    settings.setValue(networkProxyUserC, account->proxyUser());
    settings.setValue(networkUploadLimitSettingC, static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(account->uploadLimitSetting()));
    settings.setValue(networkDownloadLimitSettingC, static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(account->downloadLimitSetting()));
    settings.setValue(networkUploadLimitC, account->uploadLimit());
    settings.setValue(networkDownloadLimitC, account->downloadLimit());

    const auto proxyPasswordKey = QString(account->userIdAtHostWithPort() + networkProxyPasswordKeychainKeySuffixC);
    if (const auto proxyPassword = account->proxyPassword(); proxyPassword.isEmpty()) {
        const auto job = new QKeychain::DeletePasswordJob(Theme::instance()->appName(), this);
        job->setKey(proxyPasswordKey);
        connect(job, &QKeychain::Job::finished, this, [](const QKeychain::Job *const incomingJob) {
            if (incomingJob->error() == QKeychain::NoError) {
                qCInfo(lcAccountManager) << "Deleted proxy password from keychain";
            } else if (incomingJob->error() == QKeychain::EntryNotFound) {
                qCDebug(lcAccountManager) << "Proxy password not found in keychain, can't delete";
            } else {
                qCWarning(lcAccountManager) << "Failed to delete proxy password to keychain" << incomingJob->errorString();
            }
        });
        job->start();
    } else {
        const auto job = new QKeychain::WritePasswordJob(Theme::instance()->appName(), this);
        job->setKey(proxyPasswordKey);
        job->setBinaryData(proxyPassword.toUtf8());
        connect(job, &QKeychain::Job::finished, this, [](const QKeychain::Job *const incomingJob) {
            if (incomingJob->error() == QKeychain::NoError) {
                qCInfo(lcAccountManager) << "Saved proxy password to keychain";
            } else {
                qCWarning(lcAccountManager) << "Failed to save proxy password to keychain" << incomingJob->errorString();
            }
        });
        job->start();
    }

    if (account->_credentials) {
        if (saveCredentials) {
            // Only persist the credentials if the parameter is set, on migration from 1.8.x
            // we want to save the accounts but not overwrite the credentials
            // (This is easier than asynchronously fetching the credentials from keychain and then
            // re-persisting them)
            account->_credentials->persist();
        }

        const auto settingsMapKeys = account->_settingsMap.keys();
        for (const auto &key : settingsMapKeys) {
            if (!account->_settingsMap.value(key).isValid()) {
                continue;
            }

            settings.setValue(key, account->_settingsMap.value(key));
        }
        settings.setValue(QLatin1String(authTypeC), account->_credentials->authType());

        // HACK: Save http_user also as user
        const auto settingsMap = account->_settingsMap;
        if (settingsMap.contains(httpUserC) && settingsMap.value(httpUserC).isValid()) {
            settings.setValue(userC, settingsMap.value(httpUserC));
        }
    }

    // Save accepted certificates.
    settings.beginGroup(QLatin1String(generalC));
    qCInfo(lcAccountManager) << "Saving " << account->approvedCerts().count() << " unknown certs.";
    QByteArray certs;
    const auto approvedCerts = account->approvedCerts();
    for (const auto &cert : approvedCerts) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings.setValue(QLatin1String(caCertsKeyC), certs);
    }
    settings.endGroup();

    // Save cookies.
    if (account->_networkAccessManager) {
        const auto jar = qobject_cast<CookieJar *>(account->_networkAccessManager->cookieJar());
        if (jar) {
            qCInfo(lcAccountManager) << "Saving cookies." << account->cookieJarPath();
            if (!jar->save(account->cookieJarPath()))
            {
                qCWarning(lcAccountManager) << "Failed to save cookies to" << account->cookieJarPath();
            }
        }
    }
}

AccountPtr AccountManager::loadAccountHelper(QSettings &settings)
{
    const auto urlConfig = settings.value(QLatin1String(urlC));
    if (!urlConfig.isValid()) {
        // No URL probably means a corrupted entry in the account settings
        qCWarning(lcAccountManager) << "No URL for account " << settings.group();
        return AccountPtr();
    }

    const auto acc = createAccount();

    auto authType = settings.value(QLatin1String(authTypeC)).toString();

    // There was an account-type saving bug when 'skip folder config' was used
    // See owncloud#5408. This attempts to fix up the "dummy" or empty authType
    if (authType == QLatin1String(dummyAuthTypeC) || authType.isEmpty()) {
        if (settings.contains(QLatin1String(httpUserC))) {
            authType = httpAuthTypeC;
        } else if (settings.contains(QLatin1String(shibbolethUserC))) {
            authType = shibbolethAuthTypeC;
        } else if (settings.contains(webflowUserC)) {
            authType = webflowAuthTypeC;
        }
    }

    const auto overrideUrl = Theme::instance()->overrideServerUrl();
    const auto forceAuth = Theme::instance()->forceConfigAuthType();
    const auto multipleOverrideServers = Theme::instance()->multipleOverrideServers();
    if (!forceAuth.isEmpty() && !overrideUrl.isEmpty() && !multipleOverrideServers) {
        // If forceAuth is set, this might also mean the overrideURL has changed.
        // See enterprise issues #1126
        acc->setUrl(overrideUrl);
        authType = forceAuth;
    } else {
        acc->setUrl(urlConfig.toUrl());
    }

    // Migrate to webflow
    if (authType == QLatin1String(httpAuthTypeC)) {
        authType = webflowAuthTypeC;
        acc->_settingsMap.insert(QLatin1String(authTypeC), authType);

        const auto settingsChildKeys = settings.childKeys();
        for (const auto &key : settingsChildKeys) {
            if (!key.startsWith(httpAuthPrefix)) {
                continue;
            }

            const auto newkey = QString::fromLatin1(webflowAuthPrefix).append(key.mid(5));
            acc->_settingsMap.insert(newkey, settings.value(key));
        }
    }

    qCInfo(lcAccountManager) << "Account for" << acc->url() << "using auth type" << authType;

    acc->_serverVersion = settings.value(QLatin1String(serverVersionC)).toString();
    acc->_serverColor = settings.value(QLatin1String(serverColorC)).value<QColor>();
    acc->_serverTextColor = settings.value(QLatin1String(serverTextColorC)).value<QColor>();
    acc->_skipE2eeMetadataChecksumValidation = settings.value(QLatin1String(skipE2eeMetadataChecksumValidationC), {}).toBool();
    acc->_davUser = settings.value(QLatin1String(davUserC)).toString();

    acc->_settingsMap.insert(QLatin1String(userC), settings.value(userC));
    acc->setDavDisplayName(settings.value(QLatin1String(displayNameC), "").toString());
    const QString authTypePrefix = authType + "_";
    const auto settingsChildKeys = settings.childKeys();
    for (const auto &key : settingsChildKeys) {
        if (!key.startsWith(authTypePrefix)) {
            continue;
        }
        acc->_settingsMap.insert(key, settings.value(key));
    }

    acc->setCredentials(CredentialsFactory::create(authType));

    acc->setNetworkProxySetting(settings.value(networkProxySettingC).value<Account::AccountNetworkProxySetting>());
    acc->setProxyType(settings.value(networkProxyTypeC).value<QNetworkProxy::ProxyType>());
    acc->setProxyHostName(settings.value(networkProxyHostNameC).toString());
    acc->setProxyPort(settings.value(networkProxyPortC).toInt());
    acc->setProxyNeedsAuth(settings.value(networkProxyNeedsAuthC).toBool());
    acc->setProxyUser(settings.value(networkProxyUserC).toString());
    acc->setUploadLimitSetting(
        settings.value(
            networkUploadLimitSettingC,
            QVariant::fromValue(Account::AccountNetworkTransferLimitSetting::GlobalLimit)
        ).value<Account::AccountNetworkTransferLimitSetting>());
    acc->setDownloadLimitSetting(
        settings.value(
            networkDownloadLimitSettingC,
            QVariant::fromValue(Account::AccountNetworkTransferLimitSetting::GlobalLimit)
        ).value<Account::AccountNetworkTransferLimitSetting>());
    acc->setUploadLimit(settings.value(networkUploadLimitC).toInt());
    acc->setDownloadLimit(settings.value(networkDownloadLimitC).toInt());

    const auto proxyPasswordKey = QString(acc->userIdAtHostWithPort() + networkProxyPasswordKeychainKeySuffixC);
    const auto job = new QKeychain::ReadPasswordJob(Theme::instance()->appName(), this);
    job->setKey(proxyPasswordKey);
    connect(job, &QKeychain::Job::finished, this, [acc](const QKeychain::Job *const incomingJob) {
        const auto incomingReadJob = qobject_cast<const QKeychain::ReadPasswordJob *>(incomingJob);
        if (incomingReadJob->error() == QKeychain::NoError) {
            qCInfo(lcAccountManager) << "Read proxy password to keychain for" << acc->userIdAtHostWithPort();
            const auto passwordData = incomingReadJob->binaryData();
            const auto password = QString::fromUtf8(passwordData);
            acc->setProxyPassword(password);
        } else {
            qCWarning(lcAccountManager) << "Failed to read proxy password to keychain" << incomingJob->errorString();
        }
    });
    job->start();

    acc->setEncryptionCertificateFingerprint(settings.value(QLatin1String(encryptionCertificateSha256FingerprintC)).toByteArray());

    // now the server cert, it is in the general group
    settings.beginGroup(QLatin1String(generalC));
    const auto certs = QSslCertificate::fromData(settings.value(caCertsKeyC).toByteArray());
    qCInfo(lcAccountManager) << "Restored: " << certs.count() << " unknown certs.";
    acc->setApprovedCerts(certs);
    settings.endGroup();

    return acc;
}

AccountStatePtr AccountManager::account(const QString &name)
{
    const auto it = std::find_if(_accounts.cbegin(), _accounts.cend(), [name](const auto &acc) {
        return acc->account()->displayName() == name;
    });
    return it != _accounts.cend() ? *it : AccountStatePtr();
}

AccountStatePtr AccountManager::accountFromUserId(const QString &id) const
{
    const auto accountsList = accounts();
    for (const auto &account : accountsList) {
        const auto isUserIdWithPort = id.split(QLatin1Char(':')).size() > 1;
        const auto port = isUserIdWithPort ? account->account()->url().port() : -1;
        const auto portString = (port > 0 && port != 80 && port != 443) ? QStringLiteral(":%1").arg(port) : QStringLiteral("");
        const QString davUserId = QStringLiteral("%1@%2").arg(account->account()->davUser(), account->account()->url().host()) + portString;

        if (davUserId == id) {
            return account;
        }
    }
    return {};
}

AccountState *AccountManager::addAccount(const AccountPtr &newAccount)
{
    auto id = newAccount->id();
    if (id.isEmpty() || !isAccountIdAvailable(id)) {
        id = generateFreeAccountId();
    }
    newAccount->_id = id;

    const auto newAccountState = new AccountState(newAccount);
    addAccountState(newAccountState);
    return newAccountState;
}

void AccountManager::deleteAccount(OCC::AccountState *account)
{
    const auto it = std::find(_accounts.begin(), _accounts.end(), account);
    if (it == _accounts.end()) {
        return;
    }
    const auto copy = *it; // keep a reference to the shared pointer so it does not delete it just yet
    _accounts.erase(it);

    // Forget account credentials, cookies
    account->account()->credentials()->forgetSensitiveData();
    QFile::remove(account->account()->cookieJarPath());

    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->remove(account->account()->id());

    // Forget E2E keys
    account->account()->e2e()->forgetSensitiveData(account->account());

    account->account()->deleteAppToken();

    // clean up config from subscriptions if the account removed was the only with valid subscription
    if (account->account()->serverHasValidSubscription()) {
        updateServerHasValidSubscriptionConfig();
    }

    emit accountSyncConnectionRemoved(account);
    emit accountRemoved(account);
}

void AccountManager::updateServerHasValidSubscriptionConfig()
{
    auto serverHasValidSubscription = false;
    for (const auto &account : _accounts) {
        if (!account->account()->serverHasValidSubscription()) {
            continue;
        }

        serverHasValidSubscription = true;
        break;
    }

    ConfigFile().setServerHasValidSubscription(serverHasValidSubscription);
}

AccountPtr AccountManager::createAccount()
{
    const auto acc = Account::create();
    acc->setSslErrorHandler(new SslDialogErrorHandler);
    connect(acc.data(), &Account::proxyAuthenticationRequired,
        ProxyAuthHandler::instance(), &ProxyAuthHandler::handleProxyAuthenticationRequired);
    connect(acc.data(), &Account::lockFileError,
        Systray::instance(), &Systray::showErrorMessageDialog);

    return acc;
}

void AccountManager::shutdown()
{
    const auto accountsCopy = _accounts;
    _accounts.clear();
    for (const auto &acc : accountsCopy) {
        emit accountRemoved(acc.data());
        emit removeAccountFolders(acc.data());
    }
}

QList<AccountStatePtr> AccountManager::accounts() const
{
     return _accounts;
}

bool AccountManager::isAccountIdAvailable(const QString &id) const
{
    if (_additionalBlockedAccountIds.contains(id))
        return false;

    return std::none_of(_accounts.cbegin(), _accounts.cend(), [id](const auto &acc) {
        return acc->account()->id() == id;
    });
}

QString AccountManager::generateFreeAccountId() const
{
    auto i = 0;
    forever {
        const auto id = QString::number(i);
        if (isAccountIdAvailable(id)) {
            return id;
        }
        ++i;
    }
}

void AccountManager::addAccountState(AccountState *const accountState)
{
    Q_ASSERT(accountState);
    Q_ASSERT(accountState->account());

    QObject::connect(accountState->account().data(), &Account::wantsAccountSaved, this, &AccountManager::saveAccount);
    QObject::connect(accountState->account().data(), &Account::capabilitiesChanged, this, &AccountManager::capabilitiesChanged);

    AccountStatePtr ptr(accountState);
    _accounts << ptr;
    ptr->trySignIn();

    // update config subscriptions if the account added is the only with valid subscription
    if (accountState->account()->serverHasValidSubscription() && !ConfigFile().serverHasValidSubscription()) {
        updateServerHasValidSubscriptionConfig();
    }

    emit accountAdded(accountState);
}

bool AccountManager::forceLegacyImport() const
{
    return _forceLegacyImport;
}

void AccountManager::setForceLegacyImport(const bool forceLegacyImport)
{
    if (_forceLegacyImport == forceLegacyImport) {
        return;
    }

    _forceLegacyImport = forceLegacyImport;
    Q_EMIT forceLegacyImportChanged();
}
}
