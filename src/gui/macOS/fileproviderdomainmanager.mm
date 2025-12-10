/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <FileProvider/FileProvider.h>

#include <QDir>
#include <QLatin1StringView>
#include <QLoggingCategory>
#include <QUuid>

#include "config.h"
#include "fileproviderdomainmanager.h"
#include "fileprovidersettingscontroller.h"
#include "fileproviderutils.h"

#include "gui/accountmanager.h"
#include "libsync/account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProviderDomainManager, "nextcloud.gui.macfileproviderdomainmanager", QtInfoMsg)

}

namespace OCC {

namespace Mac {

class FileProviderDomainManager::MacImplementation
{
public:
    MacImplementation() = default;
    ~MacImplementation() = default;

    // MARK: - Synchronous NSFileProviderDomainManager Wrappers

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager addDomain:]`.
     */
    void addDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Adding domain" << domain.identifier;
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        [NSFileProviderManager addDomain:domain completionHandler:^(NSError * const error) {
            if(error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error adding domain:"
                                                          << error.code
                                                          << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            qCDebug(lcMacFileProviderDomainManager) << "Added domain with identifier" << domain.identifier;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
    }

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager disconnectWithReason:]`.
     */
    void disconnect(NSFileProviderDomain *domain, const QString &message)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Disconnecting domain" << domain.identifier;
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];

        [manager disconnectWithReason:message.toNSString() options:NSFileProviderManagerDisconnectionOptionsTemporary completionHandler:^(NSError * const error) {
            if (error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error disconnecting domain"
                                                          << domain.displayName
                                                          << error.code
                                                          << error.localizedDescription;

                dispatch_group_leave(dispatchGroup);
                return;
            }

            qCInfo(lcMacFileProviderDomainManager) << "Successfully disconnected domain"
                                                   << domain.displayName;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
    }

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager getDomainsWithCompletionHandler:]`.
     */
    NSArray<NSFileProviderDomain *> *getDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Getting all existing domains...";
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        __block NSArray<NSFileProviderDomain *> *returnValue = [NSArray array];

        [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
            if (error) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not get existing domains because of error:"
                << error.code
                << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            for (NSFileProviderDomain * const domain in domains) {
                qCInfo(lcMacFileProviderDomainManager) << "Found domain:" << domain.identifier;
            }
            
            returnValue = domains;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);

        return returnValue;
    }

    /**
     * @brief Reconnect a specific file provider domain.
     */
    void reconnect(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect domain:"
                                               << domain.identifier;

        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        [manager reconnectWithCompletionHandler:^(NSError * const error) {
            if (error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error reconnecting domain"
                                                          << domain.identifier
                                                          << "because of error:"
                                                          << error.code
                                                          << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            qCInfo(lcMacFileProviderDomainManager) << "Successfully reconnected domain:"
                                                   << domain.identifier;

            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
        signalEnumerator(domain);
    }

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager removeDomain:]`.
     *
     * Implicitly calls `removeFileProviderDomainData`, too.
     */
    void removeDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing domain:"
                                               << domain.identifier;

        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        [NSFileProviderManager removeDomain:domain completionHandler:^(NSError *error) {
            if (error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error removing domain: "
                                                          << error.code
                                                          << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            removeFileProviderDomainData(domain.identifier);
            qCInfo(lcMacFileProviderDomainManager) << "Removed domain:"
                                                   << domain.identifier;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
    }

    void signalEnumerator(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Signaling enumerator for domain" << domain.identifier;
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier completionHandler:^(NSError * const error) {
            if (error != nil) {
                qCWarning(lcMacFileProviderDomainManager) << "Error signalling:" << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            dispatch_group_leave(dispatchGroup);
            qCInfo(lcMacFileProviderDomainManager) << "Signaled enumerator for domain" << domain.identifier;
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
    }

    // MARK: - Higher Level Domain Management

    /**
     * @brief Reconnect all existing file provider domains.
     */
    void reconnectAll()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect all domains...";
        NSArray<NSFileProviderDomain *> *domains = getDomains();

        for (NSFileProviderDomain * const domain in domains) {
            reconnect(domain);
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished reconnecting all domains.";
    }

    void removeOrphanedDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing orphaned domains...";
        const auto domains = getDomains();
        QSet<QString> configuredDomainIdentifiers;
        const auto accountStates = AccountManager::instance()->accounts();

        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();

            if (!account) {
                continue;
            }

            const auto identifier = account->fileProviderDomainIdentifier();

            if (identifier.isEmpty()) {
                continue;
            }

            configuredDomainIdentifiers.insert(identifier);
        }

        for (NSFileProviderDomain * const domain in domains) {
            const auto identifier = QString::fromNSString(domain.identifier);
            
            if (!configuredDomainIdentifiers.contains(identifier)) {
                removeDomain(domain);
            }
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished removing orphaned domains.";
    }

    void restoreMissingDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Restoring missing domains...";
        const auto domains = getDomains();
        const auto accountStates = AccountManager::instance()->accounts();
        QSet<QString> existingDomainIdentifiers;

        for (NSFileProviderDomain * const domain in domains) {
            existingDomainIdentifiers.insert(QString::fromNSString(domain.identifier));
        }

        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();

            if (!account) {
                continue;
            }

            const auto identifier = account->fileProviderDomainIdentifier();

            if (identifier.isEmpty()) {
                continue;
            }

            if (!existingDomainIdentifiers.contains(identifier)) {
                qCInfo(lcMacFileProviderDomainManager) << "Restoring missing domain with identifier"
                                                       << identifier
                                                       << "for account"
                                                       << account->userIdAtHostWithPort();

                const auto newIdentifier = addFileProviderDomain(accountState.data());

                if (!newIdentifier.isEmpty()) {
                    AccountManager::instance()->setFileProviderDomainIdentifier(account->userIdAtHostWithPort(), newIdentifier);
                }
            }
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished restoring missing domains.";
    }

    /**
     * @brief Remove all file provider domains one by one and also their associated data.
     */
    void removeAllDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all domains...";
        const auto domains = getDomains();

        for (NSFileProviderDomain * const domain in domains) {
            removeDomain(domain);
        }

        qCInfo(lcMacFileProviderDomainManager) << "Completed wipe of all domains.";
    }

    // MARK: - Legacy

    QString addFileProviderDomain(const AccountState * const accountState)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto accountId = account->userIdAtHostWithPort();
        const auto existingDomainId = account->fileProviderDomainIdentifier();

        if (!existingDomainId.isEmpty()) {
            const auto domains = getDomains();

            for (NSFileProviderDomain * const domain in domains) {
                if (existingDomainId == QString::fromNSString(domain.identifier)) {
                    qCDebug(lcMacFileProviderDomainManager) << "Domain already exists for account"
                                                            << accountId
                                                            << "with identifier"
                                                            << existingDomainId;

                    return existingDomainId;
                }
            }
        }

        const auto domainId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto domainDisplayName = account->displayName();
        NSFileProviderDomain * const domain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString() displayName:domainDisplayName.toNSString()];
        addDomain(domain);
        AccountManager::instance()->setFileProviderDomainIdentifier(accountId, domainId);

        return domainId;
    }

    void removeFileProviderDomainData(NSString * const domainIdentifier)
    {
        const auto qDomainIdentifier = QString::fromNSString(domainIdentifier);

        // Remove logs.

        auto logDirectory = OCC::Mac::FileProviderUtils::fileProviderDomainLogDirectory(qDomainIdentifier);

        if (logDirectory.exists()) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing log directory at" << logDirectory.path();
            logDirectory.removeRecursively();
        } else {
            qCInfo(lcMacFileProviderDomainManager) << "Due to lack of existence, not removing log directory at" << logDirectory.path();
        }

        // Remove support data.

        auto supportDirectory = OCC::Mac::FileProviderUtils::fileProviderDomainSupportDirectory(qDomainIdentifier);

        if (supportDirectory.exists()) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing support directory at" << supportDirectory.path();
            supportDirectory.removeRecursively();
        } else {
            qCInfo(lcMacFileProviderDomainManager) << "Due to lack of existence, not removing support directory at" << supportDirectory.path();
        }
    }
};

FileProviderDomainManager::FileProviderDomainManager(QObject * const parent)
    : QObject(parent)
{
    d.reset(new FileProviderDomainManager::MacImplementation());
}

FileProviderDomainManager::~FileProviderDomainManager() = default;

NSFileProviderDomain *FileProviderDomainManager::domainForAccount(const Account *account) const
{
    if (!d || !account) {
        return nil;
    }

    const auto identifier = account->fileProviderDomainIdentifier();

    if (identifier.isEmpty()) {
        return nil;
    }

    const auto domains = d->getDomains();

    for (NSFileProviderDomain * const domain in domains) {
        if (identifier == QString::fromNSString(domain.identifier)) {
            return domain;
        }
    }

    qCWarning(lcMacFileProviderDomainManager) << "No file provider domain found for account"
                                              << account->userIdAtHostWithPort()
                                              << "with expected identifier"
                                              << identifier;
    return nil;
}

void FileProviderDomainManager::start()
{
    ConfigFile cfg;

    updateFileProviderDomains();

    // If an account is deleted from the client, accountSyncConnectionRemoved will be
    // emitted first. So we treat accountRemoved as only being relevant to client
    // shutdowns.
    connect(AccountManager::instance(), &AccountManager::accountSyncConnectionRemoved,
            this, &FileProviderDomainManager::removeFileProviderDomainForAccount);

    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, [this](const AccountState * const accountState) {
        const auto trReason = tr("%1 application has been closed. Reopen to reconnect.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
    });

    connect(FileProviderSettingsController::instance(), &FileProviderSettingsController::vfsEnabledAccountsChanged,
            this, &FileProviderDomainManager::updateFileProviderDomains);
}

void FileProviderDomainManager::updateFileProviderDomains()
{
    qCDebug(lcMacFileProviderDomainManager) << "Updating file provider domains.";

    if (!d) {
        return;
    }

    d->removeOrphanedDomains();
    d->restoreMissingDomains();
    d->reconnectAll();

    // MARK: Completely remove all file provider domains once as part of the app sandbox migration.

    ConfigFile cfg;

    if (cfg.fileProviderDomainsAppSandboxMigrationCompleted() == false) {
        qCInfo(lcMacFileProviderDomainManager) << "App sandbox migration for file provider domains not completed yet, wiping all file provider domains.";

        d->removeAllDomains();
        cfg.removeFileProviderDomainMapping();
        FileProviderSettingsController::instance()->migrateEnabledAccountsFromUserDefaults();
        cfg.setFileProviderDomainsAppSandboxMigrationCompleted(true);

        qCInfo(lcMacFileProviderDomainManager) << "App sandbox migration for file provider domains completed.";
    }

    // MARK: Update file provider domains based on client configuration.

    const auto fileProviderEnabledAccountIds = FileProviderSettingsController::instance()->vfsEnabledAccounts();
    QStringList accountIdsOfFoundFileProviderDomains;

    const auto existingDomains = d->getDomains();
    QSet<QString> existingDomainIdentifiers;

    for (NSFileProviderDomain * const domain in existingDomains) {
        existingDomainIdentifiers.insert(QString::fromNSString(domain.identifier));
    }

    const auto accounts = AccountManager::instance()->accounts();

    for (const auto &accountState : accounts) {
        const auto account = accountState->account();

        if (!account) {
            continue;
        }

        const auto domainId = account->fileProviderDomainIdentifier();

        if (domainId.isEmpty()) {
            continue;
        }

        if (!existingDomainIdentifiers.contains(domainId)) {
            continue;
        }

        accountIdsOfFoundFileProviderDomains.append(account->userIdAtHostWithPort());
    }

    for (const auto &fileProviderEnabledAccountId : fileProviderEnabledAccountIds) {
        // If the domain has already been set up for this account, then don't set it up again.
        if (accountIdsOfFoundFileProviderDomains.contains(fileProviderEnabledAccountId)) {
            accountIdsOfFoundFileProviderDomains.removeAll(fileProviderEnabledAccountId);
            continue;
        }

        if (const auto accountState = AccountManager::instance()->accountFromUserId(fileProviderEnabledAccountId)) {
            qCDebug(lcMacFileProviderDomainManager) << "Succeed in fetching account state by account id"
                                                    << fileProviderEnabledAccountId
                                                    << ", adding file provider domain for account.";

            addFileProviderDomainForAccount(accountState.data());
        } else {
            qCWarning(lcMacFileProviderDomainManager) << "Could not fetch account state by account id"
                                                      << fileProviderEnabledAccountId
                                                      << ", removing account from list of VFS-enabled accounts.";

            FileProviderSettingsController::instance()->setVfsEnabledForAccount(fileProviderEnabledAccountId, false);
        }
    }

    for (const auto &remainingAccountId : accountIdsOfFoundFileProviderDomains) {
        qCDebug(lcMacFileProviderDomainManager) << "Orphaned file provider domain to remove found for account id"
                                                << remainingAccountId;

        const auto accountState = AccountManager::instance()->accountFromUserId(remainingAccountId);
        removeFileProviderDomainForAccount(accountState.data());
    }

    // If there are no enabled accounts, check for and remove the FileProviderExt directory
    // from the group container
    if (fileProviderEnabledAccountIds.isEmpty()) {
        const QString groupContainer = Mac::FileProviderUtils::groupContainerPath();

        if (!groupContainer.isEmpty()) {
            QDir groupDir(groupContainer);
            const QString fileProviderExtDirName = QStringLiteral("FileProviderExt");

            if (groupDir.exists(fileProviderExtDirName)) {
                const QString fileProviderExtPath = groupDir.absoluteFilePath(fileProviderExtDirName);
                QDir fileProviderExtDir(fileProviderExtPath);

                qCInfo(lcMacFileProviderDomainManager) << "No file provider enabled accounts, removing FileProviderExt directory at:"
                                                       << fileProviderExtPath;

                if (fileProviderExtDir.removeRecursively()) {
                    qCInfo(lcMacFileProviderDomainManager) << "Successfully removed FileProviderExt directory in groun container.";
                } else {
                    qCWarning(lcMacFileProviderDomainManager) << "Failed to remove FileProviderExt directory at:"
                                                              << fileProviderExtPath;
                }
            }
        } else {
            qCWarning(lcMacFileProviderDomainManager) << "Could not determine group container path!";
        }
    }

    emit domainSetupComplete();
}

QString FileProviderDomainManager::addFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return {};
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    const auto identifier = d->addFileProviderDomain(accountState);

    // Disconnect the domain when something changes regarding authentication
    connect(accountState, &AccountState::stateChanged, this, [this, accountState] {
        slotAccountStateChanged(accountState);
    });

    return identifier;
}

void FileProviderDomainManager::signalEnumeratorChanged(const Account * const account)
{
    if (!d) {
        return;
    }

    Q_ASSERT(account);
    NSFileProviderDomain * const domain = domainForAccount(account);

    if (!domain) {
        return;
    }

    d->signalEnumerator(domain);
}

void FileProviderDomainManager::removeFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();

    if (!account) {
        return;
    }

    NSFileProviderDomain * const domain = domainForAccount(account.data());

    if (!domain) {
        return;
    }

    d->removeDomain(domain);
}

void FileProviderDomainManager::disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &reason)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);
    NSFileProviderDomain * const domain = domainForAccount(account.data());

    if (!domain) {
        return;
    }

    d->disconnect(domain, reason);
}

void FileProviderDomainManager::reconnectFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);
    NSFileProviderDomain * const domain = domainForAccount(account.data());

    if (!domain) {
        return;
    }

    d->reconnect(domain);
}

void FileProviderDomainManager::slotAccountStateChanged(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto state = accountState->state();

    qCDebug(lcMacFileProviderDomainManager) << "Account state changed for account:"
                                            << accountState->account()->displayName()
                                            << "changing connection status of file provider domain.";

    switch(state) {
    case AccountState::Disconnected:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::ServiceUnavailable:
    case AccountState::MaintenanceMode:
        // Do nothing, File Provider will by itself figure out connection issue
        break;
    case AccountState::SignedOut:
    case AccountState::AskingCredentials:
    case AccountState::RedirectDetected:
    case AccountState::NeedToSignTermsOfService:
    {
        // Disconnect File Provider domain while unauthenticated
        const auto trReason = tr("This account is not authenticated. Please check your account state in the %1 application.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
        break;
    }
    case AccountState::Connected:
        // Provide credentials
        reconnectFileProviderDomainForAccount(accountState);
        break;
    }
}

} // namespace Mac

} // namespace OCC
