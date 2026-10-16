/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <AppKit/AppKit.h>
#import <FileProvider/FileProvider.h>

#include <QDir>
#include <QHash>
#include <QLatin1StringView>
#include <QList>
#include <QLoggingCategory>
#include <QUuid>

#include "config.h"
#include "fileprovider.h"
#include "fileproviderdomainmanager.h"
#include "fileprovidersettingscontroller.h"
#include "fileproviderutils.h"
#include "fileproviderutils_mac.h"
#include "fileproviderxpc_mac_utils.h"

#include "gui/accountmanager.h"
#include "libsync/account.h"

static_assert(__has_feature(objc_arc), "fileproviderdomainmanager requires ARC.");

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
     * @return true when the domain was added, false when the system reported an error.
     */
    bool addDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Adding domain" << domain.identifier;
        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        __block bool addSucceeded = false;

        [NSFileProviderManager addDomain:domain completionHandler:^(NSError * const error) {
            if(error) {
                qCWarning(lcMacFileProviderDomainManager) << "Error adding domain:"
                                                          << error.code
                                                          << error.localizedDescription;
                dispatch_group_leave(dispatchGroup);
                return;
            }

            qCDebug(lcMacFileProviderDomainManager) << "Added domain with identifier" << domain.identifier;
            addSucceeded = true;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
        return addSucceeded;
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
    QList<NSFileProviderDomain *> getDomains(bool *success = nullptr)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Getting all existing domains...";
        const auto domains = FileProviderUtils::fileProviderDomains(success);

        QList<NSFileProviderDomain *> domainsList;
        for (NSFileProviderDomain *const domain in domains) {
            qCInfo(lcMacFileProviderDomainManager) << "Found domain:" << domain.identifier;
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
     * @brief Ask the extension of a domain to delete its stored password while it can still run.
     *
     * Removal goes ahead on failure because a leftover item is keyed by this domain only.
     */
    void deletePassword(NSFileProviderDomain *domain)
    {
        const auto domainIdentifier = QString::fromNSString(domain.identifier);

        if (!FileProviderXPCUtils::deletePassword(domain, FileProvider::instance()->service()).value_or(false)) {
            qCWarning(lcMacFileProviderDomainManager) << "Removing domain without confirmed password deletion" << domainIdentifier;
        }
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

        deletePassword(domain);

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
            return {};
        }
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return {};
        }

        const auto url = FileProviderUtils::userVisibleURLForItem(manager, NSFileProviderRootContainerItemIdentifier);
        return QString::fromNSString(url.path);
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
     * @brief Update every registered domain's display name to match its account's `shortcutName()`.
     *
     * Walks every registered `NSFileProviderDomain`, looks up its owning account, and if the
     * domain's `displayName` differs from `account->shortcutName()` re-registers the domain
     * in place by calling `addDomain:` again with the same identifier. macOS replaces the
     * registration, keeping the on-disk data intact.
     */
    void reconcileDomainDisplayNames()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Reconciling file provider domain display names...";

        const auto domains = getDomains();
        const auto accountManager = AccountManager::instance();

        for (NSFileProviderDomain * const domain : domains) {
            const auto domainId = QString::fromNSString(domain.identifier);
            const auto accountState = accountManager->accountFromFileProviderDomainIdentifier(domainId);

            if (!accountState || !accountState->account()) {
                // Orphan domain — not owned by any current account. `removeOrphanedDomains()`
                // in the settings controller is responsible for cleanup; nothing to do here.
                continue;
            }

            const auto currentDisplayName = QString::fromNSString(domain.displayName);
            const auto desiredDisplayName = accountState->account()->shortcutName();

            if (currentDisplayName == desiredDisplayName) {
                continue;
            }

            qCInfo(lcMacFileProviderDomainManager) << "Updating display name for domain"
                                                   << domainId
                                                   << "from" << currentDisplayName
                                                   << "to" << desiredDisplayName;

            NSFileProviderDomain * const updatedDomain = [[NSFileProviderDomain alloc] initWithIdentifier:domain.identifier displayName:desiredDisplayName.toNSString()];
            updatedDomain.supportsSyncingTrash = YES;
            addDomain(updatedDomain);
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished reconciling file provider domain display names.";
    }

    /**
     * @brief Remove all file provider domains one by one and also their associated data.
     */
    bool removeAllDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all domains...";
        auto success = false;
        const auto domains = getDomains(&success);
        if (!success) {
            return false;
        }
        for (NSFileProviderDomain * const domain : domains) {
            if (!removeDomain(domain).has_value()) {
                success = false;
            }
        }
        return success;
    }

    // MARK: - Legacy

    QString addFileProviderDomain(const AccountState * const accountState)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto accountId = account->userIdAtHostWithPort();
        const auto existingDomainId = account->fileProviderDomainIdentifier();
        // Use `<server>[:port] - <user>` ordering so the trailing token is the user. This avoids
        // LaunchServices treating the file provider root folder as a bundle when the server's TLD
        // matches a recognised package extension like ".app" (see issues #7979 / #9684).
        // Domains registered with a stale display name (e.g. the legacy U+2024-escaped form, or
        // a name from before `prettyName()` was updated) are normalised on launch by
        // `reconcileDomainDisplayNames()`.
        const auto domainDisplayName = account->shortcutName();

        const auto pendingDomainId = _pendingAddIdentifiers.value(accountId);
        auto discoverySucceeded = false;
        const auto domains = getDomains(&discoverySucceeded);
        if (!discoverySucceeded) {
            return {};
        }
        for (const auto &identifier : {existingDomainId, pendingDomainId}) {
            if (identifier.isEmpty()) {
                continue;
            }
            for (NSFileProviderDomain *const domain : domains) {
                if (identifier == QString::fromNSString(domain.identifier)) {
                    AccountManager::instance()->setFileProviderDomainIdentifier(accountId, identifier);
                    _pendingAddIdentifiers.remove(accountId);
                    return identifier;
                }
            }
        }

        auto domainId = pendingDomainId;
        if (domainId.isEmpty()) {
            domainId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        // Keep retries on the same identifier: a timed-out add may still succeed later.
        _pendingAddIdentifiers.insert(accountId, domainId);
        NSFileProviderDomain * const domain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString() displayName:domainDisplayName.toNSString()];
        domain.supportsSyncingTrash = YES;

        if (!addDomain(domain)) {
            qCWarning(lcMacFileProviderDomainManager) << "Failed to add file provider domain for account"
                                                      << accountId
                                                      << "- not persisting domain identifier.";
            return {};
        }

        AccountManager::instance()->setFileProviderDomainIdentifier(accountId, domainId);
        _pendingAddIdentifiers.remove(accountId);

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

private:
    QHash<QString, QString> _pendingAddIdentifiers;
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

QList<NSFileProviderDomain *> FileProviderDomainManager::getDomains(bool *success) const
{
    if (!d) {
        if (success) {
            *success = false;
        }
        return {};
    }

    return d->getDomains(success);
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

void FileProviderDomainManager::reconcileDomainDisplayNames()
{
    if (!d) {
        return;
    }

    d->reconcileDomainDisplayNames();
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

    if (identifier.isEmpty()) {
        return {};
    }

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

void FileProviderDomainManager::openFileViewerForDomainIdentifier(const QString &domainIdentifier) const
{
    const auto url = userVisibleUrlForDomainIdentifier(domainIdentifier);

    if (url.isEmpty()) {
        return;
    }

    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[[NSURL fileURLWithPath:url.toNSString()]]];
}

void FileProviderDomainManager::clearInsufficientQuotaErrorAndEnumerate(const QString &domainIdentifier) const
{
    if (!d || domainIdentifier.isEmpty()) {
        return;
    }

    NSFileProviderDomain *targetDomain = nil;

    for (NSFileProviderDomain * const domain : d->getDomains()) {
        if (domainIdentifier == QString::fromNSString(domain.identifier)) {
            targetDomain = domain;
            break;
        }
    }

    if (!targetDomain) {
        qCWarning(lcMacFileProviderDomainManager) << "No file provider domain found with identifier"
                                                  << domainIdentifier
                                                  << "— cannot clear insufficient-quota error.";
        return;
    }

    NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:targetDomain];

    if (!manager) {
        qCWarning(lcMacFileProviderDomainManager) << "No NSFileProviderManager for domain"
                                                  << domainIdentifier
                                                  << "— cannot clear insufficient-quota error.";
        return;
    }

    NSError * const insufficientQuotaError = [NSError errorWithDomain:NSFileProviderErrorDomain
                                                                 code:NSFileProviderErrorInsufficientQuota
                                                             userInfo:nil];

    qCInfo(lcMacFileProviderDomainManager) << "Clearing insufficient-quota error and signalling working-set enumerator for domain"
                                           << domainIdentifier;

    // Per Apple's documentation for `signalErrorResolved(_:completionHandler:)`, calling it
    // tells the system that the previously-returned error no longer applies; the system may
    // then retry the failed operation. Signalling the working set enumerator afterwards
    // nudges the system to actually re-evaluate pending non-uploaded items, mirroring the
    // pattern already used for `.notAuthenticated` in `FileProviderExtension.swift`.
    [manager signalErrorResolved:insufficientQuotaError completionHandler:^(NSError * const resolveError) {
        if (resolveError) {
            qCWarning(lcMacFileProviderDomainManager) << "Failed to clear insufficient-quota error for domain"
                                                      << domainIdentifier
                                                      << ":" << QString::fromNSString(resolveError.localizedDescription);
        }
        
        [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier
                                          completionHandler:^(NSError * const enumerateError) {
            if (enumerateError) {
                qCWarning(lcMacFileProviderDomainManager) << "Failed to signal working-set enumerator for domain"
                                                          << domainIdentifier
                                                          << ":" << QString::fromNSString(enumerateError.localizedDescription);
            }
        }];
    }];
}

void FileProviderDomainManager::slotHandleFileIdsChanged(const OCC::Account * const account, const QList<qint64> &fileIds)
{
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

    const auto fileProvider = FileProvider::instance();
    const auto xpc = fileProvider ? fileProvider->xpc() : nullptr;
    const auto domainIdentifier = QString::fromNSString(domain.identifier);

    if (xpc && xpc->processFileIdsChanged(domainIdentifier, fileIds)) {
        return;
    }

    qCWarning(lcMacFileProviderDomainManager) << "Could not forward file ID changes to domain"
                                               << domainIdentifier
                                               << "refreshing without filtering.";
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

    if (account->fileProviderDomainIdentifier().isEmpty()) {
        return QString{};
    }

    auto success = false;
    const auto domains = d->getDomains(&success);
    for (NSFileProviderDomain *const domain : domains) {
        if (account->fileProviderDomainIdentifier() == QString::fromNSString(domain.identifier)) {
            return d->removeDomain(domain);
        }
    }
    return QString{};
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
