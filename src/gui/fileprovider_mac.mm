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


#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

#include "application.h"
#include "fileprovider.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProvider, "nextcloud.gui.macfileprovider")

namespace Mac {

FileProvider *FileProvider::_instance = nullptr;

class FileProvider::Private {
  public:
    Private()
    {
    }

    ~Private() = default;

    void addFileProviderDomain(const AccountState *accountState)
    {
        const QString accountDisplayName = accountState->account()->displayName();
        const QString accountId = accountState->account()->id();

        qCDebug(lcMacFileProvider) << "Adding new file provider domain for account with id: " << accountId;

        if(_registeredDomains.contains(accountId) && _registeredDomains.value(accountId) != nil) {
            qCDebug(lcMacFileProvider) << "File provider domain for account with id already exists: " << accountId;
            return;
        }

        NSFileProviderDomain *fileProviderDomain = [[NSFileProviderDomain alloc] initWithIdentifier:accountId.toNSString() displayName:accountDisplayName.toNSString()];
        [NSFileProviderManager addDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProvider) << "Error adding file provider domain: " << [error code] << [error localizedDescription];
            }
        }];

        _registeredDomains.insert(accountId, fileProviderDomain);
    }

    void removeFileProviderDomain(const AccountState *accountState)
    {
        const QString accountId = accountState->account()->id();
        qCDebug(lcMacFileProvider) << "Removing file provider domain for account with id: " << accountId;

        if(!_registeredDomains.contains(accountId)) {
            qCDebug(lcMacFileProvider) << "File provider domain not found for id: " << accountId;
            return;
        }

        NSFileProviderDomain* fileProviderDomain = _registeredDomains[accountId];

        [NSFileProviderManager removeDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProvider) << "Error removing file provider domain: " << [error code] << [error localizedDescription];
            }
        }];

        NSFileProviderDomain* domain = _registeredDomains.take(accountId);
        [domain release];
    }

    void removeAllFileProviderDomains()
    {
        qCDebug(lcMacFileProvider) << "Removing all file provider domains.";

        [NSFileProviderManager removeAllDomainsWithCompletionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProvider) << "Error removing all file provider domains: " << [error code] << [error localizedDescription];
            }
        }];
    }

    void setFileProviderDomainConnected(const AccountState *accountState)
    {
        const QString accountDisplayName = accountState->account()->displayName();
        const QString accountId = accountState->account()->id();
        const bool accountIsConnected = accountState->isConnected();

        qCDebug(lcMacFileProvider) << "Account state for account changed: " << accountDisplayName << accountIsConnected;

        if(!_registeredDomains.contains(accountId)) {
            qCDebug(lcMacFileProvider) << "File provider domain not found for id: " << accountId;
            return;
        }

        NSFileProviderDomain* accountDomain = _registeredDomains.value(accountId);
        NSFileProviderManager* providerManager = [NSFileProviderManager managerForDomain:accountDomain];

        if(accountIsConnected) {
            [providerManager reconnectWithCompletionHandler:^(NSError *error) {
                if(error) {
                    qCDebug(lcMacFileProvider) << "Error reconnecting file provider domain: " << accountDisplayName << [error code] << [error localizedDescription];
                }
            }];
        } else {
            NSString* reason = @"Nextcloud account disconnected.";
            const auto isTemporary = accountState->state() != AccountState::SignedOut && accountState->state() != AccountState::ConfigurationError;
            NSFileProviderManagerDisconnectionOptions disconnectOption = isTemporary ? 0 : NSFileProviderManagerDisconnectionOptionsTemporary;

            [providerManager disconnectWithReason:reason options:disconnectOption completionHandler:^(NSError *error) {
                if(error) {
                    qCDebug(lcMacFileProvider) << "Error disconnecting file provider domain: " << accountDisplayName << [error code] << [error localizedDescription];
                }
            }];
        }
    }

private:
    QHash<QString, NSFileProviderDomain*> _registeredDomains;
};

FileProvider::FileProvider(QObject *parent)
    : QObject(parent)
{
    d.reset(new FileProvider::Private());

    connect(AccountManager::instance(), &AccountManager::accountAdded,
            this, &FileProvider::addFileProviderDomainForAccount);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, &FileProvider::removeFileProviderDomainForAccount);

    setupFileProviderDomains(); // Initially fetch accounts in manager
}

FileProvider::~FileProvider()
{
    d->removeAllFileProviderDomains();
}

FileProvider *FileProvider::instance()
{
    if (!_instance) {
        _instance = new FileProvider();
    }
    return _instance;
}

void FileProvider::setupFileProviderDomains()
{
    for(auto &accountState : AccountManager::instance()->accounts()) {
        addFileProviderDomainForAccount(accountState.data());
    }
}

void FileProvider::addFileProviderDomainForAccount(AccountState *accountState)
{
    d->addFileProviderDomain(accountState);

    connect(accountState, &AccountState::isConnectedChanged,
            this, [this, accountState]{ setFileProviderForAccountIsConnected(accountState); });
}

void FileProvider::removeFileProviderDomainForAccount(AccountState* accountState)
{
    d->removeFileProviderDomain(accountState);
}

void FileProvider::setFileProviderForAccountIsConnected(AccountState *accountState)
{
    d->setFileProviderDomainConnected(accountState);
}

} // namespace Mac
} // namespace OCC
