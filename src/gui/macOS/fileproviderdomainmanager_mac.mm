/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <FileProvider/FileProvider.h>

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

// Ensure that conversion to/from domain identifiers and display names
// are consistent throughout these classes
namespace {

QString uuidDomainIdentifierForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    const auto accountId = account->userIdAtHostWithPort();
    
    if (accountId.isEmpty()) {
        qCWarning(OCC::lcMacFileProviderDomainManager) << "Cannot generate UUID for account with empty userIdAtHostWithPort";
        return {};
    }
    
    // Try to get existing UUID mapping first
    OCC::ConfigFile cfg;
    const QString existingUuid = cfg.fileProviderDomainUuidFromAccountId(accountId);

    if (!existingUuid.isEmpty()) {
        qCDebug(OCC::lcMacFileProviderDomainManager) << "Using existing UUID for account:" 
                                                     << accountId
                                                     << "UUID:"
                                                     << existingUuid;

        return existingUuid;
    }
    
    // Generate new UUID for this account
    const QString newUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    qCInfo(OCC::lcMacFileProviderDomainManager) << "Generated new UUID for account:"
                                                << accountId
                                                << "UUID:"
                                                << newUuid;

    cfg.setFileProviderDomainUuidForAccountId(accountId, newUuid);
    return newUuid;
}

inline QString uuidDomainIdentifierForAccount(const OCC::AccountPtr account)
{
    return uuidDomainIdentifierForAccount(account.get());
}

inline QString domainDisplayNameForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    return account->displayName();
}

inline QString domainDisplayNameForAccount(const OCC::AccountPtr account)
{
    return domainDisplayNameForAccount(account.get());
}

inline QString accountIdFromDomainId(const QString &domainId)
{
    if (domainId.isEmpty()) {
        return {};
    }
    
    // Check if this is a UUID-based domain identifier
    if (QUuid::fromString(domainId).isNull() == false) {
        // This is a UUID, look up the account ID from the mapping
        qCDebug(OCC::lcMacFileProviderDomainManager) << "Resolving UUID-based domain ID:"
                                                     << domainId;

        OCC::ConfigFile cfg;
        const QString accountId = cfg.accountIdFromFileProviderDomainUuid(domainId);

        if (!accountId.isEmpty()) {
            qCDebug(OCC::lcMacFileProviderDomainManager) << "UUID maps to account:"
                                                         << accountId;

            return accountId;
        }

        qCWarning(OCC::lcMacFileProviderDomainManager) << "Could not find account id for UUID-based domain id:"
                                                       << domainId;

        return {};
    }
    
    // This is a legacy account-based domain identifier
    qCDebug(OCC::lcMacFileProviderDomainManager) << "Using legacy account-based domain ID:"
                                                 << domainId;

    return domainId;
}

QString accountIdFromDomainId(NSString * const domainId)
{
    if (!domainId) {
        return {};
    }
    
    auto qDomainId = QString::fromNSString(domainId);

    if (qDomainId.isEmpty()) {
        return {};
    }
    
    // Check if this is a UUID-based domain identifier
    if (QUuid::fromString(qDomainId).isNull() == false) {
        // This is a UUID, look up the account ID from the mapping
        qCDebug(OCC::lcMacFileProviderDomainManager) << "Resolving UUID-based domain ID from NSString:" << qDomainId;
        OCC::ConfigFile cfg;
        const QString accountId = cfg.accountIdFromFileProviderDomainUuid(qDomainId);

        if (!accountId.isEmpty()) {
            qCDebug(OCC::lcMacFileProviderDomainManager) << "UUID maps to account:" << accountId;
            return accountId;
        }

        qCWarning(OCC::lcMacFileProviderDomainManager) << "Could not find account id for UUID-based domain id:" << qDomainId;

        return {};
    }
    
    // This is a legacy account-based domain identifier - handle the old logic
    qCDebug(OCC::lcMacFileProviderDomainManager) << "Processing legacy account-based domain ID from NSString:" << qDomainId;

    if (!qDomainId.contains('-')) {
        return qDomainId.replace("(.)", ".");
    }

    // Using slashes as the replacement for illegal chars was unwise and we now have to pay the
    // price of doing so...
    const auto accounts = OCC::AccountManager::instance()->accounts();

    for (const auto &accountState : accounts) {
        const auto account = accountState->account();
        const auto convertedDomainId = OCC::Mac::FileProviderUtils::domainIdentifierForAccount(account);

        if (convertedDomainId == qDomainId) {
            return account->userIdAtHostWithPort();
        }
    }

    qCWarning(OCC::lcMacFileProviderDomainManager) << "Could not find account id for domain id:" << qDomainId;

    return {};
}

API_AVAILABLE(macos(11.0))
inline QString accountIdFromDomain(NSFileProviderDomain * const domain)
{
    return accountIdFromDomainId(domain.identifier);
}

}

namespace OCC {

namespace Mac {

class API_AVAILABLE(macos(11.0)) FileProviderDomainManager::MacImplementation
{
public:
    MacImplementation() = default;
    ~MacImplementation() = default;

    void findExistingFileProviderDomains()
    {
        if (@available(macOS 11.0, *)) {
            // Wait for this to finish
            dispatch_group_t dispatchGroup = dispatch_group_create();
            dispatch_group_enter(dispatchGroup);

            [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Could not get existing file provider domains: "
                                                              << error.code
                                                              << error.localizedDescription;
                    dispatch_group_leave(dispatchGroup);
                    return;
                }

                if (domains.count == 0) {
                    qCInfo(lcMacFileProviderDomainManager) << "Found no existing file provider domains at all.";
                    dispatch_group_leave(dispatchGroup);
                    return;
                }

                for (NSFileProviderDomain * const domain in domains) {
                    const auto accountId = accountIdFromDomain(domain);

                    if (const auto accountState = AccountManager::instance()->accountFromUserId(accountId);
                            accountState &&
                            accountState->account() &&
                            domainDisplayNameForAccount(accountState->account()) == QString::fromNSString(domain.displayName)) {

                        qCInfo(lcMacFileProviderDomainManager) << "Found existing file provider domain with identifier:"
                                                               << domain.identifier
                                                               << "and display name:"
                                                               << domain.displayName
                                                               << "for account:"
                                                               << accountState->account()->displayName();
                        [domain retain];

                        if (OCC::Mac::FileProviderUtils::illegalDomainIdentifier(QString::fromNSString(domain.identifier))) {
                            qCWarning(lcMacFileProviderDomainManager) << "Found existing file provider domain with illegal domain identifier:"
                                                                      << domain.identifier
                                                                      << "and display name:"
                                                                      << domain.displayName
                                                                      << "removing and recreating";
                            [NSFileProviderManager removeDomain:domain completionHandler:^(NSError * const error) {
                                if (error) {
                                    qCWarning(lcMacFileProviderDomainManager) << "Error removing file provider domain with illegal domain identifier: "
                                                                              << error.code
                                                                              << error.localizedDescription;
                                    
                                } else {
                                    qCInfo(lcMacFileProviderDomainManager) << "Successfully removed file provider domain with illegal domain identifier: "
                                                                           << domain.identifier;
                                }
                                [domain release];
                            }];
                            return;
                        }
                        
                        _registeredDomains.insert(accountId, domain);

                        NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:domain];
                        [fpManager reconnectWithCompletionHandler:^(NSError * const error) {
                            if (error) {
                                qCWarning(lcMacFileProviderDomainManager) << "Error reconnecting file provider domain: "
                                                                          << domain.displayName
                                                                          << error.code
                                                                          << error.localizedDescription;
                                return;
                            }

                            qCInfo(lcMacFileProviderDomainManager) << "Successfully reconnected file provider domain: "
                                                                   << domain.identifier
                                                                   << "and display name:"
                                                                   << domain.displayName;
                        }];

                    } else {
                        qCInfo(lcMacFileProviderDomainManager) << "Found existing file provider domain with no known configured account:"
                                                               << domain.displayName
                                                               << accountState
                                                               << (accountState ? "NON-NULL ACCOUNTSTATE" : "NULL")
                                                               << (accountState && accountState->account() ? domainDisplayNameForAccount(accountState->account()) : "NULL");
                        [NSFileProviderManager removeDomain:domain completionHandler:^(NSError * const error) {
                            if (error) {
                                qCWarning(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                                          << error.code
                                                                          << error.localizedDescription;
                            }
                        }];
                    }
                }

                dispatch_group_leave(dispatchGroup);
            }];

            dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
        }
    }

    NSFileProviderDomain *domainForAccount(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto accountId = account->userIdAtHostWithPort();
            if (_registeredDomains.contains(accountId)) {
                return _registeredDomains[accountId];
            }
        }

        return nil;
    }

    void addFileProviderDomain(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto domainDisplayName = domainDisplayNameForAccount(account);
            const auto domainId = uuidDomainIdentifierForAccount(account);  // Use UUID for new domains
            const auto accountId = account->userIdAtHostWithPort();

            qCInfo(lcMacFileProviderDomainManager) << "Adding new file provider domain with UUID: "
                                                   << domainId
                                                   << "for account:"
                                                   << accountId;

            // Check if we already have a domain for this account (by account ID, not domain ID)
            if (_registeredDomains.contains(accountId) && _registeredDomains.value(accountId) != nil) {
                qCDebug(lcMacFileProviderDomainManager) << "File provider domain already exists for account: "
                                                        << accountId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString()
                                                                                                   displayName:domainDisplayName.toNSString()];

            [NSFileProviderManager addDomain:fileProviderDomain completionHandler:^(NSError * const error) {
                if(error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error adding file provider domain: "
                                                              << error.code
                                                              << error.localizedDescription;
                }

                _registeredDomains.insert(accountId, fileProviderDomain);  // Store by account ID for easier lookup
            }];
        }
    }

    void removeFileProviderDomain(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto accountId = account->userIdAtHostWithPort();
            qCInfo(lcMacFileProviderDomainManager) << "Removing file provider domain for account: "
                                                   << accountId;

            if (!_registeredDomains.contains(accountId)) {
                qCWarning(lcMacFileProviderDomainManager) << "File provider domain not found for account: "
                                                          << accountId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[accountId];

            [NSFileProviderManager removeDomain:fileProviderDomain completionHandler:^(NSError *error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                              << error.code
                                                              << error.localizedDescription;
                }

                NSFileProviderDomain * const domain = _registeredDomains.take(accountId);
                [domain release];

                // Clean up the UUID mapping when removing the domain
                OCC::ConfigFile cfg;
                cfg.removeFileProviderDomainUuidMapping(accountId);
            }];
        }
    }

    void removeAllFileProviderDomains()
    {
        if (@available(macOS 11.0, *)) {
            qCDebug(lcMacFileProviderDomainManager) << "Removing all file provider domains.";

            [NSFileProviderManager removeAllDomainsWithCompletionHandler:^(NSError * const error) {
                if(error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error removing all file provider domains: "
                                                            << error.code
                                                            << error.localizedDescription;
                    return;
                }

                const auto registeredDomainPtrs = _registeredDomains.values();
                const auto accountIds = _registeredDomains.keys();
                for (NSFileProviderDomain * const domain : registeredDomainPtrs) {
                    if (domain != nil) {
                        [domain release];
                    }
                }
                // Clean up UUID mappings for all accounts
                OCC::ConfigFile cfg;
                for (const QString &accountId : accountIds) {
                    cfg.removeFileProviderDomainUuidMapping(accountId);
                }
                _registeredDomains.clear();
            }];
        }
    }

    void wipeAllFileProviderDomains()
    {
        if (@available(macOS 12.0, *)) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all file provider domains";

            [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domains: "
                                                              << error.code
                                                              << error.localizedDescription;
                    return;
                }

                for (NSFileProviderDomain * const domain in domains) {
                    [NSFileProviderManager removeDomain:domain mode:NSFileProviderDomainRemovalModeRemoveAll completionHandler:^(NSURL * const preservedLocation, NSError * const error) {
                        Q_UNUSED(preservedLocation)

                        if (error) {
                            qCWarning(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domain: "
                                                                      << domain.displayName
                                                                      << error.code
                                                                      << error.localizedDescription;
                            return;
                        }

                        const QString accountId = accountIdFromDomainId(domain.identifier);
                        NSFileProviderDomain * const registeredDomainPtr = _registeredDomains.take(accountId);
                        if (registeredDomainPtr != nil) {
                            [domain release];
                            // Clean up UUID mapping when wiping domain
                            if (!accountId.isEmpty()) {
                                OCC::ConfigFile cfg;
                                cfg.removeFileProviderDomainUuidMapping(accountId);
                            }
                        }
                    }];
                }
            }];
        } else if (@available(macOS 11.0, *)) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing all file provider domains, can't specify wipe on macOS 11";
            removeAllFileProviderDomains();
        }
    }

    void disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &message)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto accountId = account->userIdAtHostWithPort();
            qCInfo(lcMacFileProviderDomainManager) << "Disconnecting file provider domain for account: "
                                                   << accountId;

            if(!_registeredDomains.contains(accountId)) {
                qCInfo(lcMacFileProviderDomainManager) << "File provider domain not found for account: "
                                                       << accountId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[accountId];
            Q_ASSERT(fileProviderDomain != nil);

            NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
            [fpManager disconnectWithReason:message.toNSString()
                                    options:NSFileProviderManagerDisconnectionOptionsTemporary
                          completionHandler:^(NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error disconnecting file provider domain: "
                                                              << fileProviderDomain.displayName
                                                              << error.code
                                                              << error.localizedDescription;
                    return;
                }

                qCInfo(lcMacFileProviderDomainManager) << "Successfully disconnected file provider domain: "
                                                       << fileProviderDomain.displayName;
            }];
        }
    }

    void reconnectFileProviderDomainForAccount(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto accountId = account->userIdAtHostWithPort();
            qCInfo(lcMacFileProviderDomainManager) << "Reconnecting file provider domain for account: "
                                                   << accountId;

            if(!_registeredDomains.contains(accountId)) {
                qCInfo(lcMacFileProviderDomainManager) << "File provider domain not found for account: "
                                                       << accountId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[accountId];
            Q_ASSERT(fileProviderDomain != nil);

            NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
            [fpManager reconnectWithCompletionHandler:^(NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error reconnecting file provider domain: "
                                                              << fileProviderDomain.displayName
                                                              << error.code
                                                              << error.localizedDescription;
                    return;
                }

                qCInfo(lcMacFileProviderDomainManager) << "Successfully reconnected file provider domain: "
                                                       << fileProviderDomain.displayName;

                signalEnumeratorChanged(account.get());
            }];
        }
    }

    void signalEnumeratorChanged(const Account * const account)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(account);
            const auto accountId = account->userIdAtHostWithPort();

            qCInfo(lcMacFileProviderDomainManager) << "Signalling enumerator changed in file provider domain for account: "
                                                   << accountId;

            if(!_registeredDomains.contains(accountId)) {
                qCInfo(lcMacFileProviderDomainManager) << "File provider domain not found for account: "
                                                       << accountId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[accountId];
            Q_ASSERT(fileProviderDomain != nil);

            NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
            [fpManager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier completionHandler:^(NSError * const error) {
                if (error != nil) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error signalling enumerator changed for working set:"
                                                              << error.localizedDescription;
                }
            }];
        }
    }

    QStringList configuredDomainIds() const
    {
        return _registeredDomains.keys();
    }

private:
    QHash<QString, NSFileProviderDomain*> _registeredDomains;
};

FileProviderDomainManager::FileProviderDomainManager(QObject * const parent)
    : QObject(parent)
{
    if (@available(macOS 11.0, *)) {
        d.reset(new FileProviderDomainManager::MacImplementation());
    } else {
        qCWarning(lcMacFileProviderDomainManager()) << "Trying to run File Provider on system that does not support it.";
    }
}

FileProviderDomainManager::~FileProviderDomainManager() = default;

void FileProviderDomainManager::start()
{
    ConfigFile cfg;

    setupFileProviderDomains();

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

void FileProviderDomainManager::setupFileProviderDomains()
{
    if (!d) {
        return;
    }

    d->findExistingFileProviderDomains();
    updateFileProviderDomains();
}

void FileProviderDomainManager::updateFileProviderDomains()
{
    qCDebug(lcMacFileProviderDomainManager) << "Updating file provider domains.";

    if (!d) {
        return;
    }

    const auto vfsEnabledAccounts = FileProviderSettingsController::instance()->vfsEnabledAccounts();
    auto configuredDomains = d->configuredDomainIds();

    for (const auto &accountUserIdAtHost : vfsEnabledAccounts) {
        // If the domain has already been set up for this account, then don't set it up again
        if (configuredDomains.contains(accountUserIdAtHost)) {
            configuredDomains.removeAll(accountUserIdAtHost);
            continue;
        }

        if (const auto accountState = AccountManager::instance()->accountFromUserId(accountUserIdAtHost)) {
            qCDebug(lcMacFileProviderDomainManager) << "Succeed in fetching account state by account id"
                                                    << accountUserIdAtHost
                                                    << ", adding file provider domain for account.";

            addFileProviderDomainForAccount(accountState.data());
        } else {
            qCWarning(lcMacFileProviderDomainManager) << "Could not fetch account state by account id"
                                                      << accountUserIdAtHost
                                                      << ", removing account from list of VFS-enabled accounts.";

            FileProviderSettingsController::instance()->setVfsEnabledForAccount(accountUserIdAtHost, false);
        }
    }

    for (const auto &remainingDomainUserId : configuredDomains) {
        const auto accountId = accountIdFromDomainId(remainingDomainUserId);
        const auto accountState = AccountManager::instance()->accountFromUserId(accountId);
        removeFileProviderDomainForAccount(accountState.data());
    }

    emit domainSetupComplete();
}

void FileProviderDomainManager::addFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->addFileProviderDomain(accountState);

    // Disconnect the domain when something changes regarding authentication
    connect(accountState, &AccountState::stateChanged, this, [this, accountState] {
        slotAccountStateChanged(accountState);
    });
}

void FileProviderDomainManager::signalEnumeratorChanged(const Account * const account)
{
    if (!d) {
        return;
    }

    Q_ASSERT(account);
    d->signalEnumeratorChanged(account);
}

void FileProviderDomainManager::removeFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->removeFileProviderDomain(accountState);
}

void FileProviderDomainManager::disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &reason)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->disconnectFileProviderDomainForAccount(accountState, reason);
}

void FileProviderDomainManager::reconnectFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();

    d->reconnectFileProviderDomainForAccount(accountState);
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

AccountStatePtr FileProviderDomainManager::accountStateFromFileProviderDomainIdentifier(const QString &domainIdentifier)
{
    if (domainIdentifier.isEmpty()) {
        qCWarning(lcMacFileProviderDomainManager) << "Cannot return accountstateptr for empty domain identifier";
        return AccountStatePtr();
    }

    const auto accountUserId = accountIdFromDomainId(domainIdentifier);
    const auto accountForReceivedDomainIdentifier = AccountManager::instance()->accountFromUserId(accountUserId);
    if (!accountForReceivedDomainIdentifier) {
        qCWarning(lcMacFileProviderDomainManager) << "Could not find account matching user id matching file provider domain identifier:"
                                                  << domainIdentifier;
    }

    return accountForReceivedDomainIdentifier;
}

QString FileProviderDomainManager::fileProviderDomainIdentifierFromAccountState(const AccountStatePtr &accountState)
{
    return uuidDomainIdentifierForAccount(accountState->account());
}

void* FileProviderDomainManager::domainForAccount(const AccountState * const accountState)
{
    return d->domainForAccount(accountState);
}

} // namespace Mac

} // namespace OCC
