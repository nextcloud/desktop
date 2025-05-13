/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "libsync/clientproxy.h"

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

// The maximum versions that this client can read
constexpr auto maxAccountsVersion = 13;
constexpr auto maxAccountVersion = 13;

constexpr auto serverHasValidSubscriptionC = "serverHasValidSubscription";

constexpr auto generalC = "General";
}


namespace OCC {

Q_LOGGING_CATEGORY(lcAccountManager, "nextcloud.gui.account.manager", QtInfoMsg)

AccountManager *AccountManager::instance()
{
    static AccountManager instance;
    return &instance;
}

AccountManager::AccountsRestoreResult AccountManager::restore(const QString &legacyConfigFile, const bool alsoRestoreLegacySettings)
{
    QStringList skipSettingsKeys;
    backwardMigrationSettingsKeys(&skipSettingsKeys, &skipSettingsKeys);

    const auto isLegacyMigration = !legacyConfigFile.isEmpty();
    auto settings = isLegacyMigration ? std::make_unique<QSettings>(legacyConfigFile, QSettings::IniFormat)
                                      : ConfigFile::settingsWithGroup(QLatin1String(accountsC));

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

    qCWarning(lcAccountManager) << "Restoring settings" << settings->allKeys().join(", ") << "from" << (isLegacyMigration ? legacyConfigFile
                                                                                                                          : ConfigFile().configFile());
    // migrate general and proxy settings
    if (isLegacyMigration && alsoRestoreLegacySettings) {
        ConfigFile configFile;
        configFile.setVfsEnabled(settings->value(configFile.isVfsEnabledC).toBool());
        configFile.setLaunchOnSystemStartup(settings->value(configFile.launchOnSystemStartupC).toBool());
        configFile.setOptionalServerNotifications(settings->value(configFile.optionalServerNotificationsC).toBool());
        configFile.setPromptDeleteFiles(settings->value(configFile.promptDeleteC).toBool());
        configFile.setShowCallNotifications(settings->value(configFile.showCallNotificationsC).toBool());
        configFile.setShowChatNotifications(settings->value(configFile.showChatNotificationsC).toBool());
        configFile.setShowInExplorerNavigationPane(settings->value(configFile.showInExplorerNavigationPaneC).toBool());
        ClientProxy().saveProxyConfigurationFromSettings(*settings);
    }

    auto result = isLegacyMigration ? AccountsRestoreSuccess : AccountsRestoreSuccessFromLegacyVersion;
    settings->beginGroup(accountsC);
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
    settings->endGroup();

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

void AccountManager::setupAccountsAndFolders()
{
    const auto accountsRestoreResult = restoreLegacyAccount();

    const auto foldersListSize = FolderMan::instance()->setupFolders();

    const auto prettyNamesList = [](const QList<AccountStatePtr> &accounts) {
        QStringList list;
        for (const auto &account : accounts) {
            list << account->account()->prettyName().prepend("- ");
        }
        return list.join("\n");
    };

    if (const auto accounts = AccountManager::instance()->accounts();
        accountsRestoreResult == AccountManager::AccountsRestoreSuccessFromLegacyVersion
        && !accounts.isEmpty()) {

        const auto accountsListSize = accounts.size();
        if (Theme::instance()->displayLegacyImportDialog()) {
            const auto accountsRestoreMessage = accountsListSize > 1
                ? tr("%1 accounts", "number of accounts imported").arg(QString::number(accountsListSize))
                : tr("1 account");
            const auto foldersRestoreMessage = foldersListSize > 1
                ? tr("%1 folders", "number of folders imported").arg(QString::number(foldersListSize))
                : tr("1 folder");
            const auto messageBox = new QMessageBox(QMessageBox::Information,
                                                    tr("Legacy import"),
                                                    tr("Imported %1 and %2 from a legacy desktop client.\n%3",
                                                       "number of accounts and folders imported. list of users.")
                                                        .arg(accountsRestoreMessage,
                                                             foldersRestoreMessage,
                                                             prettyNamesList(accounts))
                                                    );
            messageBox->setWindowModality(Qt::NonModal);
            messageBox->open();
        }

        qCWarning(lcAccountManager) << "Migration result AccountManager::AccountsRestoreResult:" << accountsRestoreResult;
        qCWarning(lcAccountManager) << "Folders migrated: " << foldersListSize;
        qCWarning(lcAccountManager) << accountsListSize << "account(s) were migrated:" << prettyNamesList(accounts);

    } else {
        qCWarning(lcAccountManager) << "Migration result AccountManager::AccountsRestoreResult: " << accountsRestoreResult;
        qCWarning(lcAccountManager) << "Folders migrated: " << foldersListSize;
        qCWarning(lcAccountManager) << "No accounts were migrated, prompting user to set up accounts and folders from scratch.";
    }
}

AccountManager::AccountsRestoreResult AccountManager::restoreLegacyAccount()
{
    ConfigFile configFile;
    const auto tryMigrate = configFile.overrideServerUrl().isEmpty();

    auto accountsRestoreResult = AccountManager::AccountsRestoreFailure;
    const auto legacyConfigFile = ConfigFile().findLegacyConfigFile();
    if (legacyConfigFile.isEmpty()) {
        return accountsRestoreResult;
    }

    const auto displayLegacyImportDialog = Theme::instance()->displayLegacyImportDialog();
    auto oCSettings = std::make_unique<QSettings>(legacyConfigFile, QSettings::IniFormat);
    oCSettings->beginGroup(QLatin1String(accountsC));
    const auto accountsListSize = oCSettings->childGroups().size();
    oCSettings->endGroup();

    auto showDialogs = false;
    if (!forceLegacyImport() && accountsListSize > 0 && displayLegacyImportDialog) {
        showDialogs = true;
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
            QMessageBox::information(nullptr,
                                     tr("Legacy import"),
                                     tr("Could not import accounts from legacy client configuration."));
            return accountsRestoreResult;
        }

        if (accountsRestoreResult = restore(legacyConfigFile, tryMigrate);
            accountsRestoreResult == AccountManager::AccountsRestoreFailure) {
            // If there is an error reading the account settings, try again
            // after a couple of seconds, if that fails, give up.
            // (non-existence is not an error)
            Utility::sleep(5);
            if (accountsRestoreResult = AccountManager::instance()->restore(legacyConfigFile, tryMigrate);
                accountsRestoreResult == AccountManager::AccountsRestoreFailure
                && showDialogs) {
                qCCritical(lcAccountManager) << "Could not read the account settings, quitting";
                QMessageBox::critical(
                    nullptr,
                    tr("Error accessing the configuration file"),
                    tr("There was an error while accessing the configuration "
                       "file at %1. Please make sure the file can be accessed by your system account.")
                        .arg(ConfigFile().configFile()),
                    QMessageBox::Ok
                    );
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            }
        }
    }

    return accountsRestoreResult;
}

}
