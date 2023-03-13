/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#import <FileProvider/FileProvider.h>

#include <QLoggingCategory>

#include "config.h"
#include "fileproviderdomainmanager.h"
#include "pushnotifications.h"

#include "gui/accountmanager.h"
#include "libsync/account.h"

namespace {

QString domainIdentifierForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    return account->userIdAtHostWithPort();
}

QString domainIdentifierForAccount(const OCC::AccountPtr account)
{
    return domainIdentifierForAccount(account.get());
}

QString domainDisplayNameForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    return account->displayName();
}

QString domainDisplayNameForAccount(const OCC::AccountPtr account)
{
    return domainDisplayNameForAccount(account.get());
}

QString accountIdFromDomain(NSFileProviderDomain * const domain)
{
    return QString::fromNSString(domain.identifier);
}

}

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProviderDomainManager, "nextcloud.gui.macfileproviderdomainmanager", QtInfoMsg)

namespace Mac {

FileProviderDomainManager *FileProviderDomainManager::_instance = nullptr;

class FileProviderDomainManager::Private {

  public:
    Private() = default;
    ~Private() = default;

    void findExistingFileProviderDomains()
    {
        // Wait for this to finish
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Could not get existing file provider domains: "
                                                        << error.code
                                                        << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            if (domains.count == 0) {
                qCDebug(lcMacFileProviderDomainManager) << "Found no existing file provider domains";
                dispatch_group_leave(dispatchGroup);
                return;
            }

            for (NSFileProviderDomain * const domain in domains) {
                const auto accountId = accountIdFromDomain(domain);

                if (const auto accountState = AccountManager::instance()->accountFromUserId(accountId);
                        accountState &&
                        accountState->account() &&
                        domainDisplayNameForAccount(accountState->account()) == QString::fromNSString(domain.displayName)) {

                    qCDebug(lcMacFileProviderDomainManager) << "Found existing file provider domain for account:"
                                                            << accountState->account()->displayName();
                    [domain retain];
                    _registeredDomains.insert(accountId, domain);

                    NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:domain];
                    [fpManager reconnectWithCompletionHandler:^(NSError * const error) {
                        if (error) {
                            qCDebug(lcMacFileProviderDomainManager) << "Error reconnecting file provider domain: "
                                                                    << domain.displayName
                                                                    << error.code
                                                                    << error.localizedDescription;
                            return;
                        }

                        qCDebug(lcMacFileProviderDomainManager) << "Successfully reconnected file provider domain: "
                                                                << domain.displayName;
                    }];

                } else {
                    qCDebug(lcMacFileProviderDomainManager) << "Found existing file provider domain with no known configured account:"
                                                            << domain.displayName;
                    [NSFileProviderManager removeDomain:domain completionHandler:^(NSError * const error) {
                        if(error) {
                            qCDebug(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
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

    void addFileProviderDomain(const AccountState * const accountState)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto domainDisplayName = domainDisplayNameForAccount(account);
        const auto domainId = domainIdentifierForAccount(account);

        qCDebug(lcMacFileProviderDomainManager) << "Adding new file provider domain with id: " << domainId;

        if(_registeredDomains.contains(domainId) && _registeredDomains.value(domainId) != nil) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain with id already exists: " << domainId;
            return;
        }

        NSFileProviderDomain * const fileProviderDomain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString()
                                                                                               displayName:domainDisplayName.toNSString()];
        [fileProviderDomain retain];

        [NSFileProviderManager addDomain:fileProviderDomain completionHandler:^(NSError * const error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error adding file provider domain: "
                                                        << error.code
                                                        << error.localizedDescription;
            }

            _registeredDomains.insert(domainId, fileProviderDomain);
        }];
    }

    void removeFileProviderDomain(const AccountState * const accountState)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto domainId = domainIdentifierForAccount(account);
        qCDebug(lcMacFileProviderDomainManager) << "Removing file provider domain with id: " << domainId;

        if(!_registeredDomains.contains(domainId)) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain not found for id: " << domainId;
            return;
        }

        NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];

        [NSFileProviderManager removeDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                        << error.code
                                                        << error.localizedDescription;
            }

            NSFileProviderDomain * const domain = _registeredDomains.take(domainId);
            [domain release];
        }];
    }

    void removeAllFileProviderDomains()
    {
        qCDebug(lcMacFileProviderDomainManager) << "Removing all file provider domains.";

        [NSFileProviderManager removeAllDomainsWithCompletionHandler:^(NSError * const error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error removing all file provider domains: "
                                                        << error.code
                                                        << error.localizedDescription;
                return;
            }

            const auto registeredDomainPtrs = _registeredDomains.values();
            for (NSFileProviderDomain * const domain : registeredDomainPtrs) {
                if (domain != nil) {
                    [domain release];
                }
            }
            _registeredDomains.clear();
        }];
    }

    void wipeAllFileProviderDomains()
    {
        qCDebug(lcMacFileProviderDomainManager) << "Removing and wiping all file provider domains";

        if (@available(macOS 12.0, *)) {
            [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
                if (error) {
                    qCDebug(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domains: "
                                                            << error.code
                                                            << error.localizedDescription;
                    return;
                }

                for (NSFileProviderDomain * const domain in domains) {
                    [NSFileProviderManager removeDomain:domain mode:NSFileProviderDomainRemovalModeRemoveAll completionHandler:^(NSURL * const preservedLocation, NSError * const error) {
                        Q_UNUSED(preservedLocation)

                        if (error) {
                            qCDebug(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domain: "
                                                                    << domain.displayName
                                                                    << error.code
                                                                    << error.localizedDescription;
                            return;
                        }

                        NSFileProviderDomain * const registeredDomainPtr = _registeredDomains.take(QString::fromNSString(domain.identifier));
                        if (registeredDomainPtr != nil) {
                            [domain release];
                        }
                    }];
                }
            }];
        } else {
            removeAllFileProviderDomains();
        }
    }

    void readdFileProviderDomain(NSFileProviderDomain * const domain)
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            // Wait for this to finish
            dispatch_group_t dispatchGroup = dispatch_group_create();
            dispatch_group_notify(dispatchGroup, dispatch_get_main_queue(), ^{
                [NSFileProviderManager addDomain:domain completionHandler:^(NSError * const error) {
                    if(error) {
                        qCDebug(lcMacFileProviderDomainManager) << "Error adding file provider domain: "
                                                                << error.code
                                                                << error.localizedDescription;
                    }
                }];
            });

            dispatch_group_enter(dispatchGroup);
            [NSFileProviderManager removeDomain:domain completionHandler:^(NSError * const error) {
                if(error) {
                    qCDebug(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                            << error.code
                                                            << error.localizedDescription;
                }

                dispatch_group_leave(dispatchGroup);
            }];
        });
    }

    void disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &message)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto domainId = domainIdentifierForAccount(account);
        qCDebug(lcMacFileProviderDomainManager) << "Removing file provider domain with id: " << domainId;

        if(!_registeredDomains.contains(domainId)) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain not found for id: " << domainId;
            return;
        }

        NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];
        Q_ASSERT(fileProviderDomain != nil);

        NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
        if (fpManager == nil) {
            readdFileProviderDomain(fileProviderDomain);
            return;
        }

        [fpManager disconnectWithReason:message.toNSString()
                                options:NSFileProviderManagerDisconnectionOptionsTemporary
                      completionHandler:^(NSError * const error) {
            if (error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error disconnecting file provider domain: "
                                                        << fileProviderDomain.displayName
                                                        << error.code
                                                        << error.localizedDescription;
                return;
            }

            qCDebug(lcMacFileProviderDomainManager) << "Successfully disconnected file provider domain: "
                                                    << fileProviderDomain.displayName;
        }];
    }

    void reconnectFileProviderDomainForAccount(const AccountState * const accountState)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto domainId = domainIdentifierForAccount(account);
        qCDebug(lcMacFileProviderDomainManager) << "Removing file provider domain with id: " << domainId;

        if(!_registeredDomains.contains(domainId)) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain not found for id: " << domainId;
            return;
        }

        NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];
        Q_ASSERT(fileProviderDomain != nil);

        NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
        if (fpManager == nil) {
            readdFileProviderDomain(fileProviderDomain);
            return;
        }

        [fpManager reconnectWithCompletionHandler:^(NSError * const error) {
            if (error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error reconnecting file provider domain: "
                                                        << fileProviderDomain.displayName
                                                        << error.code
                                                        << error.localizedDescription;
                return;
            }

            qCDebug(lcMacFileProviderDomainManager) << "Successfully reconnected file provider domain: "
                                                    << fileProviderDomain.displayName;
        }];
    }

    void signalEnumeratorChanged(const Account * const account)
    {
        Q_ASSERT(account);
        const auto domainId = domainIdentifierForAccount(account);

        qCDebug(lcMacFileProviderDomainManager) << "Signalling enumerator changed in file provider domain for account with id: " << domainId;

        if(!_registeredDomains.contains(domainId)) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain not found for id: " << domainId;
            return;
        }

        NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];
        Q_ASSERT(fileProviderDomain != nil);

        NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
        if (fpManager == nil) {
            readdFileProviderDomain(fileProviderDomain);
            return;
        }

        [fpManager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier completionHandler:^(NSError * const error) {
            if (error != nil) {
                qCDebug(lcMacFileProviderDomainManager) << "Error signalling enumerator changed for working set:"
                                                        << error.localizedDescription;
            }
        }];
    }

private:
    QHash<QString, NSFileProviderDomain*> _registeredDomains;
};

FileProviderDomainManager::FileProviderDomainManager(QObject * const parent)
    : QObject(parent)
{
    d.reset(new FileProviderDomainManager::Private());

    setupFileProviderDomains();

    connect(AccountManager::instance(), &AccountManager::accountAdded,
            this, &FileProviderDomainManager::addFileProviderDomainForAccount);
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
}

FileProviderDomainManager *FileProviderDomainManager::instance()
{
    if (!_instance) {
        _instance = new FileProviderDomainManager();
    }
    return _instance;
}

FileProviderDomainManager::~FileProviderDomainManager() = default;

void FileProviderDomainManager::setupFileProviderDomains()
{
    d->findExistingFileProviderDomains();

    for(auto &accountState : AccountManager::instance()->accounts()) {
        addFileProviderDomainForAccount(accountState.data());
    }
}

void FileProviderDomainManager::addFileProviderDomainForAccount(const AccountState * const accountState)
{
    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->addFileProviderDomain(accountState);

    // Disconnect the domain when something changes regarding authentication
    connect(accountState, &AccountState::stateChanged, this, [this, accountState] {
        slotAccountStateChanged(accountState);
    });

    // Setup push notifications
    const auto accountCapabilities = account->capabilities().isValid();
    if (!accountCapabilities) {
        connect(account.get(), &Account::capabilitiesChanged, this, [this, account] {
            trySetupPushNotificationsForAccount(account.get());
        });
        return;
    }

    trySetupPushNotificationsForAccount(account.get());
}

void FileProviderDomainManager::trySetupPushNotificationsForAccount(const Account * const account)
{
    Q_ASSERT(account);

    const auto pushNotifications = account->pushNotifications();
    const auto pushNotificationsCapability = account->capabilities().availablePushNotifications() & PushNotificationType::Files;

    if (pushNotificationsCapability && pushNotifications && pushNotifications->isReady()) {
        qCDebug(lcMacFileProviderDomainManager) << "Push notifications already ready, connecting them to enumerator signalling."
                                                << account->displayName();
        setupPushNotificationsForAccount(account);
    } else if (pushNotificationsCapability) {
        qCDebug(lcMacFileProviderDomainManager) << "Push notifications not yet ready, will connect to signalling when ready."
                                                << account->displayName();
        connect(account, &Account::pushNotificationsReady, this, &FileProviderDomainManager::setupPushNotificationsForAccount);
    }
}

void FileProviderDomainManager::setupPushNotificationsForAccount(const Account * const account)
{
    Q_ASSERT(account);

    qCDebug(lcMacFileProviderDomainManager) << "Setting up push notifications for file provider domain for account:"
                                            << account->displayName();

    connect(account->pushNotifications(), &PushNotifications::filesChanged, this, &FileProviderDomainManager::signalEnumeratorChanged);
    disconnect(account, &Account::pushNotificationsReady, this, &FileProviderDomainManager::setupPushNotificationsForAccount);
}

void FileProviderDomainManager::signalEnumeratorChanged(const Account * const account)
{
    Q_ASSERT(account);
    d->signalEnumeratorChanged(account);
}

void FileProviderDomainManager::removeFileProviderDomainForAccount(const AccountState * const accountState)
{
    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->removeFileProviderDomain(accountState);

    const auto pushNotifications = account->pushNotifications();
    const auto pushNotificationsCapability = account->capabilities().availablePushNotifications() & PushNotificationType::Files;

    if (pushNotificationsCapability && pushNotifications && pushNotifications->isReady()) {
        disconnect(pushNotifications, &PushNotifications::filesChanged, this, &FileProviderDomainManager::signalEnumeratorChanged);
    } else if (pushNotificationsCapability) {
        disconnect(account.get(), &Account::pushNotificationsReady, this, &FileProviderDomainManager::setupPushNotificationsForAccount);
    }
}

void FileProviderDomainManager::disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &reason)
{
    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->disconnectFileProviderDomainForAccount(accountState, reason);
}

void FileProviderDomainManager::reconnectFileProviderDomainForAccount(const AccountState * const accountState)
{
    Q_ASSERT(accountState);
    const auto account = accountState->account();

    d->reconnectFileProviderDomainForAccount(accountState);
}

void FileProviderDomainManager::slotAccountStateChanged(const AccountState * const accountState)
{
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
