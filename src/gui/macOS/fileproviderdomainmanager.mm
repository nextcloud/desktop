/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <FileProvider/FileProvider.h>

#include <QDir>
#include <QLatin1StringView>
#include <QList>
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
    QList<NSFileProviderDomain *> getDomains()
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

            if (domains.count > 0) {
                for (NSFileProviderDomain * const domain in domains) {
                    qCInfo(lcMacFileProviderDomainManager) << "Found domain:" << domain.identifier;
                }
            } else {
                qCInfo(lcMacFileProviderDomainManager) << "Found no existing domains.";
            }

            // Ensure the array (and contained domains) stay retained after the completion block returns.
            returnValue = [domains copy];
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);

        QList<NSFileProviderDomain *> domainsList;

        for (NSFileProviderDomain * const domain in returnValue) {
            domainsList.append(domain);
        }

        return domainsList;
    }

    /**
     * @brief Reconnect a specific file provider domain.
     */
    void reconnect(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect domain"
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

            qCInfo(lcMacFileProviderDomainManager) << "Successfully reconnected domain"
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
     * 
     * @return The path to the location where preserved dirty user data is stored, or an empty QString if none.
     */
    QString removeDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing domain"
                                               << domain.identifier;

        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);
        
        __block NSURL *preservedDataURL = nil;

        [NSFileProviderManager removeDomain:domain mode:NSFileProviderDomainRemovalModePreserveDirtyUserData completionHandler:^(NSURL * const dataURL, NSError * const error) {
            if (error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error removing domain"
                                                          << error.code
                                                          << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            if (dataURL) {
                preservedDataURL = [dataURL copy];
                qCInfo(lcMacFileProviderDomainManager) << "Domain removed with preserved data at:" << dataURL.path;
            } else {
                qCInfo(lcMacFileProviderDomainManager) << "Domain removed with no preserved data";
            }

            removeFileProviderDomainData(domain.identifier);
            qCInfo(lcMacFileProviderDomainManager) << "Removed domain"
                                                   << domain.identifier;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
        
        if (preservedDataURL) {
            return QString::fromNSString(preservedDataURL.path);
        }
        
        return {};
    }

    void signalEnumerator(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Signaling enumerator for domain" << domain.identifier;
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier completionHandler:^(NSError * const error) {
            if (error != nil) {
                qCWarning(lcMacFileProviderDomainManager) << "Error signalling" << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            dispatch_group_leave(dispatchGroup);
            qCInfo(lcMacFileProviderDomainManager) << "Signaled enumerator for domain" << domain.identifier;
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
    }

    /**
     * @brief Synchronous wrapper to get the user-visible URL for a domain's root container.
     */
    QString getUserVisibleUrlForDomain(NSFileProviderDomain *domain)
    {
        if (!domain) {
            qCWarning(lcMacFileProviderDomainManager) << "Cannot get user-visible URL for nil domain";
            return {};
        }

        qCInfo(lcMacFileProviderDomainManager) << "Getting user-visible URL for domain" << domain.identifier;
        
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        __block NSURL *resultURL = nil;

        [manager getUserVisibleURLForItemIdentifier:NSFileProviderRootContainerItemIdentifier completionHandler:^(NSURL * const url, NSError * const error) {
            if (error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error getting user-visible URL for domain"
                                                          << domain.identifier
                                                          << ":"
                                                          << error.code
                                                          << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            if (url) {
                resultURL = [url copy];
                qCInfo(lcMacFileProviderDomainManager) << "Got user-visible URL for domain"
                                                       << domain.identifier
                                                       << ":"
                                                       << url.path;
            } else {
                qCWarning(lcMacFileProviderDomainManager) << "No user-visible URL returned for domain" << domain.identifier;
            }

            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);

        if (resultURL) {
            return QString::fromNSString(resultURL.path);
        }

        return {};
    }

    // MARK: - Higher Level Domain Management

    /**
     * @brief Reconnect all existing file provider domains.
     */
    void reconnectAll()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect all domains...";
        const auto domains = getDomains();

        for (NSFileProviderDomain * const domain : domains) {
            reconnect(domain);
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished reconnecting all domains.";
    }

    /**
     * @brief Remove all file provider domains one by one and also their associated data.
     */
    void removeAllDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all domains...";
        const auto domains = getDomains();

        for (NSFileProviderDomain * const domain : domains) {
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

            for (NSFileProviderDomain * const domain : domains) {
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
        // Replace dots with one-dot leader (U+2024) to prevent Finder from treating
        // folders as bundles when the display name contains extensions like ".app"
        auto domainDisplayName = account->displayName();
        domainDisplayName.replace('.', QChar(0x2024));
        NSFileProviderDomain * const domain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString() displayName:domainDisplayName.toNSString()];
        domain.supportsSyncingTrash = YES;
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

// MARK: -

FileProviderDomainManager::FileProviderDomainManager(QObject * const parent)
    : QObject(parent)
{
    qCDebug(lcMacFileProviderDomainManager) << "Initializing...";

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

    for (NSFileProviderDomain * const domain : domains) {
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
    qCDebug(lcMacFileProviderDomainManager) << "Starting...";

    ConfigFile cfg;

    // If an account is deleted from the client, accountSyncConnectionRemoved will be
    // emitted first. So we treat accountRemoved as only being relevant to client
    // shutdowns.
    connect(AccountManager::instance(), &AccountManager::accountSyncConnectionRemoved,
            this, &FileProviderDomainManager::removeDomainByAccount);

    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, [this](const AccountState * const accountState) {
        const auto trReason = tr("%1 application has been closed. Reopen to reconnect.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
    });

    qCDebug(lcMacFileProviderDomainManager) << "Completed start.";
}

QList<NSFileProviderDomain *> FileProviderDomainManager::getDomains() const
{
    if (!d) {
        return {};
    }

    return d->getDomains();
}

void FileProviderDomainManager::removeAllDomains()
{
    if (!d) {
        return;
    }

    d->removeAllDomains();
}

void FileProviderDomainManager::reconnectAll()
{
    if (!d) {
        return;
    }

    d->reconnectAll();
}

QString FileProviderDomainManager::removeDomain(NSFileProviderDomain *domain)
{
    if (!d || !domain) {
        return {};
    }

    return d->removeDomain(domain);
}

QString FileProviderDomainManager::addDomainForAccount(const AccountState * const accountState)
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

QString FileProviderDomainManager::userVisibleUrlForDomainIdentifier(const QString &domainIdentifier) const
{
    if (!d || domainIdentifier.isEmpty()) {
        return {};
    }

    const auto domains = d->getDomains();

    for (NSFileProviderDomain * const domain : domains) {
        if (domainIdentifier == QString::fromNSString(domain.identifier)) {
            return d->getUserVisibleUrlForDomain(domain);
        }
    }

    qCWarning(lcMacFileProviderDomainManager) << "No file provider domain found with identifier"
                                              << domainIdentifier;
    return {};
}

void FileProviderDomainManager::slotHandleFileIdsChanged(const OCC::Account * const account, const QList<qint64> &fileIds)
{
    // NOTE: The fileIds argument is ignored for now but retained in the signature for future use.
    // We signal the enumerator regardless of the number of changed files.
    Q_UNUSED(fileIds)

    if (!d || !account) {
        return;
    }

    qCInfo(lcMacFileProviderDomainManager) << "Received file ID changes for account"
                                            << account->displayName();

    NSFileProviderDomain * const domain = domainForAccount(account);

    if (!domain) {
        qCWarning(lcMacFileProviderDomainManager) << "No domain found for account"
                                                   << account->displayName();
        return;
    }

    // Signal the enumerator to refresh
    d->signalEnumerator(domain);
}

QString FileProviderDomainManager::removeDomainByAccount(const AccountState * const accountState)
{
    if (!d) {
        return {};
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();

    if (!account) {
        return {};
    }

    NSFileProviderDomain * const domain = domainForAccount(account.data());

    if (!domain) {
        return {};
    }

    return d->removeDomain(domain);
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
