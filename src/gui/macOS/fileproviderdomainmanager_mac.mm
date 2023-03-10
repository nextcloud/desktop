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

#include "fileproviderdomainmanager.h"

#import <FileProvider/FileProvider.h>

#include <QLoggingCategory>

#include "gui/accountmanager.h"
#include "gui/accountstate.h"
#include "libsync/account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProviderDomainManager, "nextcloud.gui.macfileproviderdomainmanager", QtInfoMsg)

namespace Mac {

FileProviderDomainManager *FileProviderDomainManager::_instance = nullptr;

class FileProviderDomainManager::Private {

  public:
    Private() = default;
    ~Private() = default;

    void addFileProviderDomain(const AccountState *accountState)
    {
        const QString accountDisplayName = accountState->account()->displayName();
        const QString accountId = accountState->account()->userIdAtHostWithPort();

        qCDebug(lcMacFileProviderDomainManager) << "Adding new file provider domain for account with id: " << accountId;

        if(_registeredDomains.contains(accountId) && _registeredDomains.value(accountId) != nil) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain for account with id already exists: " << accountId;
            return;
        }

        NSFileProviderDomain *fileProviderDomain = [[NSFileProviderDomain alloc] initWithIdentifier:accountId.toNSString()
                                                                                        displayName:accountDisplayName.toNSString()];
        [NSFileProviderManager addDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error adding file provider domain: "
                                                        << [error code]
                                                        << [error localizedDescription];
            }
        }];

        _registeredDomains.insert(accountId, fileProviderDomain);
    }

    void removeFileProviderDomain(const AccountState *accountState)
    {
        const QString accountId = accountState->account()->id();
        qCDebug(lcMacFileProviderDomainManager) << "Removing file provider domain for account with id: " << accountId;

        if(!_registeredDomains.contains(accountId)) {
            qCDebug(lcMacFileProviderDomainManager) << "File provider domain not found for id: " << accountId;
            return;
        }

        NSFileProviderDomain* fileProviderDomain = _registeredDomains[accountId];

        [NSFileProviderManager removeDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                        << [error code]
                                                        << [error localizedDescription];
            }
        }];

        NSFileProviderDomain* domain = _registeredDomains.take(accountId);
        [domain release];
    }

    void removeAllFileProviderDomains()
    {
        qCDebug(lcMacFileProviderDomainManager) << "Removing all file provider domains.";

        [NSFileProviderManager removeAllDomainsWithCompletionHandler:^(NSError *error) {
            if(error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error removing all file provider domains: "
                                                        << [error code]
                                                        << [error localizedDescription];
            }
        }];
    }

    void wipeAllFileProviderDomains()
    {
        qCDebug(lcMacFileProviderDomainManager) << "Removing and wiping all file provider domains";

        [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *domains, NSError *error) {
            if (error) {
                qCDebug(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domains: "
                                                        << [error code]
                                                        << [error localizedDescription];
                return;
            }

            for (NSFileProviderDomain *domain in domains) {
                [NSFileProviderManager removeDomain:domain mode:NSFileProviderDomainRemovalModeRemoveAll completionHandler:^(NSURL *preservedLocation, NSError *error) {
                    if (error) {
                        qCDebug(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domain: "
                                                                << [domain displayName]
                                                                << [error code]
                                                                << [error localizedDescription];
                        return;
                    }
                }];
            }
        }];
    }

private:
    QHash<QString, NSFileProviderDomain*> _registeredDomains;
};

FileProviderDomainManager::FileProviderDomainManager(QObject *parent)
    : QObject(parent)
{
    d.reset(new FileProviderDomainManager::Private());

    connect(AccountManager::instance(), &AccountManager::accountAdded,
            this, &FileProviderDomainManager::addFileProviderDomainForAccount);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, &FileProviderDomainManager::removeFileProviderDomainForAccount);

    d->wipeAllFileProviderDomains(); // TODO: Remove when testing done
    setupFileProviderDomains(); // Initially fetch accounts in manager
}

FileProviderDomainManager::~FileProviderDomainManager()
{
    d->removeAllFileProviderDomains();
}

FileProviderDomainManager *FileProviderDomainManager::instance()
{
    if (!_instance) {
        _instance = new FileProviderDomainManager();
    }
    return _instance;
}

void FileProviderDomainManager::setupFileProviderDomains()
{
    for(auto &accountState : AccountManager::instance()->accounts()) {
        addFileProviderDomainForAccount(accountState.data());
    }
}

void FileProviderDomainManager::addFileProviderDomainForAccount(AccountState *accountState)
{
    d->addFileProviderDomain(accountState);
}

void FileProviderDomainManager::removeFileProviderDomainForAccount(AccountState* accountState)
{
    d->removeFileProviderDomain(accountState);
}

} // namespace Mac

} // namespace OCC
