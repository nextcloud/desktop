/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "accountmanager.h"

#include "config.h"
#include "sslerrordialog.h"
#include "proxyauthhandler.h"
#include "creds/credentialsfactory.h"
#include "creds/abstractcredentials.h"
#include "creds/keychainchunk.h"
#include "libsync/clientsideencryption.h"
#include "libsync/configfile.h"
#include "libsync/cookiejar.h"
#include "libsync/theme.h"
#include "libsync/clientproxy.h"
#if !DISABLE_ACCOUNT_MIGRATION
#include "legacyaccountselectiondialog.h"
#endif

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
constexpr auto maxAccountsVersion = 13;
constexpr auto maxAccountVersion = 13;

constexpr auto serverHasValidSubscriptionC = "serverHasValidSubscription";
constexpr auto serverDesktopEnterpriseUpdateChannelC = "desktopEnterpriseChannel";

constexpr auto generalC = "General";
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
        emit(accountListInitialized());
        return AccountsRestoreSuccessWithSkipped;
    }

    // If there are no accounts, check the old format.
#if !DISABLE_ACCOUNT_MIGRATION
    if (settings->childGroups().isEmpty() && !settings->contains(QLatin1String(versionC)) && alsoRestoreLegacySettings) {
        if(!restoreFromLegacySettings()) {
            return AccountsNotFound;
        }

        emit(accountListInitialized());
        return AccountsRestoreSuccessFromLegacyVersion;
    }
#endif

    if (settings->childGroups().isEmpty()) {
        emit(accountListInitialized());
        return AccountsNotFound;
    }

    auto result = AccountsRestoreSuccess;
    const auto settingsChildGroups = settings->childGroups();
    for (const auto &accountId : settingsChildGroups) {
        settings->beginGroup(accountId);
        if (!skipSettingsKeys.contains(settings->group())) {
            const auto acc = loadAccountHelper(*settings);
            if (!acc) {
                continue;
            }
            acc->_id = accountId;
            const auto accState = new AccountState(acc);
            const auto jar = qobject_cast<CookieJar*>(acc->_networkAccessManager->cookieJar());
            Q_ASSERT(jar);
            if (jar) {
                jar->restore(acc->cookieJarPath());
            }
            addAccountState(accState);
            migrateNetworkSettings(acc, *settings);
            settings->endGroup();
        } else {
            qCInfo(lcAccountManager) << "Account" << accountId << "is too new, ignoring";
            _additionalBlockedAccountIds.insert(accountId);
            result = AccountsRestoreSuccessWithSkipped;
        }
    }

    emit(accountListInitialized());

    ConfigFile().cleanupGlobalNetworkConfiguration();
    ClientProxy().cleanupGlobalNetworkConfiguration();   

    return result;
}

void AccountManager::backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    const auto accountsVersion = settings->value(QLatin1String(versionC)).toInt();

    qCInfo(lcAccountManager) << "Checking for accounts versions.";
    qCInfo(lcAccountManager) << "Config accounts version:" << accountsVersion;
    qCInfo(lcAccountManager) << "Max accounts Version is set to:" << maxAccountsVersion;
    if (accountsVersion <= maxAccountsVersion) {
        const auto settingsChildGroups = settings->childGroups();
        for (const auto &accountId : settingsChildGroups) {
            settings->beginGroup(accountId);
            const auto accountVersion = settings->value(QLatin1String(versionC), 1).toInt();

            if (accountVersion > maxAccountVersion) {
                ignoreKeys->append(settings->group());
                qCInfo(lcAccountManager) << "Ignoring account" << accountId << "because of version" << accountVersion;
            }
            settings->endGroup();
        }
    } else {
        deleteKeys->append(settings->group());
    }
}
#if !DISABLE_ACCOUNT_MIGRATION
bool AccountManager::restoreFromLegacySettings()
{
    qCInfo(lcAccountManager) << "Migrate: restoreFromLegacySettings, checking settings group"
                             << Theme::instance()->appName();

    // try to open the correctly themed settings
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());

    auto wasLegacyImportDialogDisplayed = false;
    const auto displayLegacyImportDialog = Theme::instance()->displayLegacyImportDialog();
    QStringList selectedAccountIds;

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

        for (const auto &configFile : std::as_const(legacyLocations)) {
            auto oCSettings = std::make_unique<QSettings>(configFile, QSettings::IniFormat);
            if (oCSettings->status() != QSettings::Status::NoError) {
                qCInfo(lcAccountManager) << "Error reading legacy configuration file" << oCSettings->status();
                break;
            }

            oCSettings->beginGroup(QLatin1String(accountsC));
            const auto childGroups = oCSettings->childGroups();
            const auto accountsListSize = childGroups.size();
            oCSettings->endGroup(); //accountsC
            if (const QFileInfo configFileInfo(configFile);
                configFileInfo.exists() && configFileInfo.isReadable()) {

                qCInfo(lcAccountManager) << "Migrate: checking old config " << configFile;
                if (!forceLegacyImport() && accountsListSize > 0 && displayLegacyImportDialog) {
                    wasLegacyImportDialogDisplayed = true;
                    if (accountsListSize == 1) {
                        const auto importQuestion =
                            tr("An account was detected from a legacy desktop client.\n"
                               "Should the account be imported?");
                        QMessageBox importMessageBox(QMessageBox::Question, tr("Legacy import"), importQuestion);
                        importMessageBox.addButton(tr("Import"), QMessageBox::AcceptRole);
                        const auto skipButton = importMessageBox.addButton(tr("Skip"), QMessageBox::DestructiveRole);
                        importMessageBox.exec();
                        if (importMessageBox.clickedButton() == skipButton) {
                            return false;
                        }
                        selectedAccountIds = childGroups;
                    } else {
                        QVector<LegacyAccountSelectionDialog::AccountItem> accountsToDisplay;
                        oCSettings->beginGroup(QLatin1String(accountsC));
                        for (const auto &accId : childGroups) {
                            oCSettings->beginGroup(accId);
                            const auto displayName = oCSettings->value(QLatin1String(displayNameC)).toString();
                            const auto urlStr = oCSettings->value(QLatin1String(urlC)).toString();
                            oCSettings->endGroup(); //accId
                            const auto label = QString("%1 - %2").arg(displayName, urlStr);
                            accountsToDisplay.push_back({accId, label});
                        }
                        oCSettings->endGroup(); //accountsC

                        LegacyAccountSelectionDialog accountSelectionDialog(accountsToDisplay);
                        if (accountSelectionDialog.exec() != QDialog::Accepted) {
                            return false;
                        }
                        selectedAccountIds = accountSelectionDialog.selectedAccountIds();
                        if (selectedAccountIds.isEmpty()) {
                            return false;
                        }
                    }
                } else {
                    selectedAccountIds = childGroups;
                }

                const auto legacyVersion = oCSettings->value(ConfigFile::clientVersionC, {}).toString();
                ConfigFile().setClientPreviousVersionString(legacyVersion);
                qCInfo(lcAccountManager) << "Migrating from" << legacyVersion;
                qCInfo(lcAccountManager) << "Copy settings" << oCSettings->allKeys().join(", ");
                settings = std::move(oCSettings);
                ConfigFile::setDiscoveredLegacyConfigPath(configFileInfo.canonicalPath());
                break;
            } else {
                qCInfo(lcAccountManager) << "Migrate: could not read old config " << configFile;
            }
        }
    }

    ConfigFile configFile;
    // General settings
    configFile.setVfsEnabled(settings->value(ConfigFile::isVfsEnabledC, configFile.isVfsEnabled()).toBool());
    configFile.setLaunchOnSystemStartup(settings->value(ConfigFile::launchOnSystemStartupC,
                                                        configFile.launchOnSystemStartup()).toBool());
    const auto useMonoIcons = settings->value(ConfigFile::monoIconsC, configFile.monoIcons()).toBool();
    Theme::instance()->setSystrayUseMonoIcons(useMonoIcons);
    configFile.setMonoIcons(useMonoIcons);
    configFile.setOptionalServerNotifications(settings->value(ConfigFile::optionalServerNotificationsC,
                                                              configFile.optionalServerNotifications()).toBool());
    configFile.setPromptDeleteFiles(settings->value(ConfigFile::promptDeleteC,
                                                    configFile.promptDeleteFiles()).toBool());
    configFile.setShowCallNotifications(settings->value(ConfigFile::showCallNotificationsC,
                                                        configFile.showCallNotifications()).toBool());
    configFile.setShowChatNotifications(settings->value(ConfigFile::showChatNotificationsC,
                                                        configFile.showChatNotifications()).toBool());
    configFile.setShowInExplorerNavigationPane(settings->value(ConfigFile::showInExplorerNavigationPaneC,
                                                               configFile.showInExplorerNavigationPane()).toBool());
    // Advanced
    const auto newBigFolderSizeLimit = settings->value(ConfigFile::newBigFolderSizeLimitC, configFile.newBigFolderSizeLimit().second).toLongLong();
    const auto useNewBigFolderSizeLimit = settings->value(ConfigFile::useNewBigFolderSizeLimitC, configFile.useNewBigFolderSizeLimit()).toBool();
    configFile.setNewBigFolderSizeLimit(useNewBigFolderSizeLimit, newBigFolderSizeLimit);
    configFile.setNotifyExistingFoldersOverLimit(settings->value(ConfigFile::notifyExistingFoldersOverLimitC,
                                                                 configFile.notifyExistingFoldersOverLimit()).toBool());
    configFile.setStopSyncingExistingFoldersOverLimit(settings->value(ConfigFile::stopSyncingExistingFoldersOverLimitC,
                                                                      configFile.stopSyncingExistingFoldersOverLimit()).toBool());
    configFile.setConfirmExternalStorage(settings->value(ConfigFile::confirmExternalStorageC, configFile.confirmExternalStorage()).toBool());
    configFile.setMoveToTrash(settings->value(ConfigFile::moveToTrashC, configFile.moveToTrash()).toBool());
    // Info
    configFile.setUpdateChannel(settings->value(ConfigFile::updateChannelC, configFile.currentUpdateChannel()).toString());
    auto previousAppName = settings->contains(ConfigFile::legacyAppName) ? ConfigFile::legacyAppName
                                                                         : ConfigFile::unbrandedAppName;
    const auto updaterGroupName = QString("%1/%2").arg(previousAppName, ConfigFile::autoUpdateCheckC);
    configFile.setAutoUpdateCheck(settings->value(updaterGroupName, configFile.autoUpdateCheck()).toBool(), {});

    // Global Proxy and Network
    ClientProxy().saveProxyConfigurationFromSettings(*settings);
    configFile.setUseUploadLimit(settings->value(ConfigFile::useUploadLimitC, configFile.useUploadLimit()).toInt());
    configFile.setUploadLimit(settings->value(ConfigFile::uploadLimitC, configFile.uploadLimit()).toInt());
    configFile.setUseDownloadLimit(settings->value(ConfigFile::useDownloadLimitC, configFile.useDownloadLimit()).toInt());
    configFile.setDownloadLimit(settings->value(ConfigFile::downloadLimitC, configFile.downloadLimit()).toInt());

    // Try to load the single account.
    configFile.setMigrationPhase(ConfigFile::MigrationPhase::SetupUsers);
    if (!settings->childKeys().isEmpty()) {
        settings->beginGroup(accountsC);
        const auto childGroups = selectedAccountIds.isEmpty() ? settings->childGroups() : selectedAccountIds;
        auto accountsLoaded = false;
        for (const auto &accountId : childGroups) {
            settings->beginGroup(accountId);
            const auto acc = loadAccountHelper(*settings);
            if (!acc) {
                continue;
            }
            addAccount(acc);
            accountsLoaded = true;
            migrateNetworkSettings(acc, *settings);
            settings->endGroup();
        }
        configFile.cleanupGlobalNetworkConfiguration();
        ClientProxy().cleanupGlobalNetworkConfiguration();
        return accountsLoaded;
    }

    if (wasLegacyImportDialogDisplayed) {
        QMessageBox::information(nullptr,
                                 tr("Legacy import"),
                                 tr("Could not import accounts from legacy client configuration."));
    }

    return false;
}
#else
bool AccountManager::restoreFromLegacySettings()
{
    return false;
}
#endif

void AccountManager::save(bool saveCredentials)
{
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    settings->setValue(QLatin1String(versionC), maxAccountsVersion);
    for (const auto &acc : std::as_const(_accounts)) {
        settings->beginGroup(acc->account()->id());
        saveAccountHelper(acc->account(), *settings, saveCredentials);
        settings->endGroup();
    }

    settings->sync();
    qCInfo(lcAccountManager) << "Saved all account settings, status:" << settings->status();
}

void AccountManager::saveAccount(const AccountPtr &newAccountData)
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

void AccountManager::saveAccountHelper(const AccountPtr &account, QSettings &settings, bool saveCredentials)
{
    qCDebug(lcAccountManager) << "Saving settings to" << settings.fileName();
    settings.setValue(QLatin1String(versionC), maxAccountVersion);
    if (account->isPublicShareLink()) {
        settings.setValue(QLatin1String(urlC), account->publicShareLinkUrl().toString());
    } else {
        settings.setValue(QLatin1String(urlC), account->_url.toString());
    }
    settings.setValue(QLatin1String(davUserC), account->_davUser);
    settings.setValue(QLatin1String(displayNameC), account->davDisplayName());
    settings.setValue(QLatin1String(serverVersionC), account->_serverVersion);
    settings.setValue(QLatin1String(serverColorC), account->_serverColor);
    settings.setValue(QLatin1String(serverTextColorC), account->_serverTextColor);
    settings.setValue(QLatin1String(serverHasValidSubscriptionC), account->serverHasValidSubscription());
    settings.setValue(QLatin1String(serverDesktopEnterpriseUpdateChannelC), account->enterpriseUpdateChannel().toString());
    settings.setValue(QLatin1String(encryptionCertificateSha256FingerprintC), account->encryptionCertificateFingerprint());
    if (!account->_skipE2eeMetadataChecksumValidation) {
        settings.remove(QLatin1String(skipE2eeMetadataChecksumValidationC));
    } else {
        settings.setValue(QLatin1String(skipE2eeMetadataChecksumValidationC), account->_skipE2eeMetadataChecksumValidation);
    }

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

void AccountManager::migrateNetworkSettings(const AccountPtr &account, const QSettings &settings)
{
    // QSettings from old ConfigFile to new ConfigFile to Account
    auto accountProxyType = settings.value(networkProxyTypeC).value<QNetworkProxy::ProxyType>();
    auto accountProxyHost = settings.value(networkProxyHostNameC).toString();
    auto accountProxyPort = settings.value(networkProxyPortC).toInt();
    auto accountProxyNeedsAuth = settings.value(networkProxyNeedsAuthC).toBool();
    auto accountProxyUser = settings.value(networkProxyUserC).toString();

    // Override user settings with global settings if user is set to use global settings
    ConfigFile configFile;
    auto accountProxySetting = settings.value(networkProxySettingC).toInt();
    if (accountProxySetting == 0 && configFile.isMigrationInProgress()) {
        accountProxyType = static_cast<QNetworkProxy::ProxyType>(configFile.proxyType());
        accountProxyHost = configFile.proxyHostName();
        accountProxyPort = configFile.proxyPort();
        accountProxyNeedsAuth = configFile.proxyNeedsAuth();
        accountProxyUser = configFile.proxyUser();
        qCInfo(lcAccountManager) << "Account is using global settings:" << accountProxyType;
    }
    account->setProxyType(accountProxyType);
    account->setProxyHostName(accountProxyHost);
    account->setProxyPort(accountProxyPort);
    account->setProxyNeedsAuth(accountProxyNeedsAuth);
    account->setProxyUser(accountProxyUser);
    const auto globalUseUploadLimit = static_cast<Account::AccountNetworkTransferLimitSetting>(configFile.useUploadLimit());
    const auto globalUseDownloadLimit = static_cast<Account::AccountNetworkTransferLimitSetting>(configFile.useDownloadLimit());
    // User network settings
    auto userUseUploadLimit = static_cast<Account::AccountNetworkTransferLimitSetting>(settings.value(networkUploadLimitSettingC, 
        QVariant::fromValue(account->uploadLimitSetting())).toInt());
    auto userUploadLimit = settings.value(networkUploadLimitC, account->uploadLimit()).toInt();
    auto userUseDownloadLimit = static_cast<Account::AccountNetworkTransferLimitSetting>(settings.value(networkDownloadLimitSettingC, 
        QVariant::fromValue(account->downloadLimitSetting())).toInt());
    auto userDownloadLimit = settings.value(networkDownloadLimitC, account->downloadLimit()).toInt();
    if (userUseUploadLimit == Account::AccountNetworkTransferLimitSetting::LegacyGlobalLimit) {
        userUseUploadLimit = globalUseUploadLimit;
        userUploadLimit = configFile.uploadLimit();
        qCDebug(lcAccountManager) << "Overriding upload limit with global setting:" << userUseUploadLimit 
            << "- upload limit:" << userUploadLimit;
    }
    if (userUseDownloadLimit == Account::AccountNetworkTransferLimitSetting::LegacyGlobalLimit) {
        userUseDownloadLimit = globalUseDownloadLimit;
        userDownloadLimit = configFile.downloadLimit();
        qCDebug(lcAccountManager) << "Overriding download limit with global setting" << userUseDownloadLimit 
            << "- download limit:" << userDownloadLimit;
    }
    if (userUseUploadLimit != Account::AccountNetworkTransferLimitSetting::NoLimit) {
        account->setUploadLimitSetting(userUseUploadLimit);
        account->setUploadLimit(userUploadLimit);
    }
    if (userUseDownloadLimit != Account::AccountNetworkTransferLimitSetting::NoLimit) {
        account->setDownloadLimitSetting(userUseDownloadLimit);
        account->setDownloadLimit(userDownloadLimit);
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

    acc->setUploadLimitSetting(
        settings.value(
            networkUploadLimitSettingC,
            QVariant::fromValue(Account::AccountNetworkTransferLimitSetting::NoLimit)
        ).value<Account::AccountNetworkTransferLimitSetting>());
    acc->setDownloadLimitSetting(
        settings.value(
            networkDownloadLimitSettingC,
            QVariant::fromValue(Account::AccountNetworkTransferLimitSetting::NoLimit)
        ).value<Account::AccountNetworkTransferLimitSetting>());
    acc->setUploadLimit(settings.value(networkUploadLimitC).toInt());
    acc->setDownloadLimit(settings.value(networkDownloadLimitC).toInt());

    ConfigFile configFile;
    const auto proxyPasswordKey = QString(acc->userIdAtHostWithPort() + networkProxyPasswordKeychainKeySuffixC);
    const auto appName = configFile.isUnbrandedToBrandedMigrationInProgress() ? ConfigFile::unbrandedAppName 
        : Theme::instance()->appName();
    const auto job = new QKeychain::ReadPasswordJob(appName, this);
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

    if (_accounts.size() == 1) {
        emit(accountListInitialized());
    }

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
    account->account()->e2e()->forgetSensitiveData();

    account->account()->deleteAppToken();

    // clean up config from subscriptions and enterprise channel
    updateServerHasValidSubscriptionConfig();
    updateServerDesktopEnterpriseUpdateChannel();

    emit accountSyncConnectionRemoved(account);
    emit accountRemoved(account);
}

void AccountManager::updateServerHasValidSubscriptionConfig()
{
    auto serverHasValidSubscription = false;
    for (const auto &account : std::as_const(_accounts)) {
        if (account->account()->serverHasValidSubscription()) {
            serverHasValidSubscription = true;
            break;
        }
    }

    if (ConfigFile().serverHasValidSubscription() != serverHasValidSubscription) {
        ConfigFile().setServerHasValidSubscription(serverHasValidSubscription);
    }
}

void AccountManager::updateServerDesktopEnterpriseUpdateChannel()
{
    UpdateChannel most_stable_channel = UpdateChannel::Invalid;
    for (const auto &account : std::as_const(_accounts)) {
        if (const auto accounts_channel = account->account()->enterpriseUpdateChannel();
            account->account()->serverHasValidSubscription() && accounts_channel > most_stable_channel) {
            most_stable_channel = accounts_channel;
        }
    }

    ConfigFile().setDesktopEnterpriseChannel(most_stable_channel.toString());
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

    updateServerHasValidSubscriptionConfig();
    updateServerDesktopEnterpriseUpdateChannel();

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
