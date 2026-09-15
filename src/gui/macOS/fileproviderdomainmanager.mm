/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <AppKit/AppKit.h>
#import <FileProvider/FileProvider.h>

#include <QDir>
#include <QLatin1StringView>
#include <QList>
#include <QLoggingCategory>
#include <QUuid>

#include "config.h"
#include "fileprovider.h"
#include "fileproviderdomainmanager.h"
#include "fileprovidersettingscontroller.h"
#include "fileproviderutils.h"

#include "common/utility_mac_sandbox.h"
#include "gui/accountmanager.h"
#include "libsync/account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProviderDomainManager, "nextcloud.gui.macfileproviderdomainmanager", QtInfoMsg)

}

namespace OCC {

namespace Mac {

namespace
{
NSString *const fileProviderAccountIdentifierUserInfoKey = @"org.nextcloud.desktop.accountIdentifier";
}

class FileProviderDomainManager::MacImplementation
{
public:
    MacImplementation() = default;

    ~MacImplementation()
    {
        if (_domainDidChangeObserver) {
            [[NSNotificationCenter defaultCenter] removeObserver:_domainDidChangeObserver];
            _domainDidChangeObserver = nil;
        }
    }

    void startObservingDomainChanges(FileProviderDomainManager *manager)
    {
        if (_domainDidChangeObserver) {
            return;
        }

        _domainDidChangeObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSFileProviderDomainDidChange
                                                                                     object:nil
                                                                                      queue:[NSOperationQueue mainQueue]
                                                                                 usingBlock:^(__unused NSNotification *notification) {
                                                                                     qCInfo(lcMacFileProviderDomainManager)
                                                                                         << "File Provider domain list changed";
                                                                                     manager->reconcileExternalDomainMappings();
                                                                                     Q_EMIT manager->domainsChanged();
                                                                                     FileProvider::instance()->configureXPC();
                                                                                 }];
    }

    [[nodiscard]] bool externalVolumeStorageAvailable() const
    {
        if (@available(macOS 15.0, *)) {
            return true;
        }

        return false;
    }

    [[nodiscard]] NSURL *mountedVolumeUrlForUuid(const QString &volumeUuid) const
    {
        if (volumeUuid.isEmpty()) {
            return nil;
        }

        const auto volumeUrls =
            [[NSFileManager defaultManager] mountedVolumeURLsIncludingResourceValuesForKeys:@[ NSURLVolumeUUIDStringKey, NSURLVolumeNameKey ]
                                                                                    options:NSVolumeEnumerationSkipHiddenVolumes];

        for (NSURL *const volumeUrl in volumeUrls) {
            NSString *mountedVolumeUuid = nil;
            NSError *resourceError = nil;

            if (![volumeUrl getResourceValue:&mountedVolumeUuid forKey:NSURLVolumeUUIDStringKey error:&resourceError]) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not read mounted volume UUID" << resourceError.localizedDescription;
                continue;
            }

            if (volumeUuid.compare(QString::fromNSString(mountedVolumeUuid), Qt::CaseInsensitive) == 0) {
                return volumeUrl;
            }
        }

        return nil;
    }

    [[nodiscard]] QString externalVolumeDisplayNameForUuid(const QString &volumeUuid) const
    {
        const auto volumeUrl = mountedVolumeUrlForUuid(volumeUuid);
        if (!volumeUrl) {
            return {};
        }

        NSString *volumeName = nil;
        NSError *resourceError = nil;
        if (![volumeUrl getResourceValue:&volumeName forKey:NSURLVolumeNameKey error:&resourceError]) {
            qCWarning(lcMacFileProviderDomainManager) << "Could not read mounted volume name" << resourceError.localizedDescription;
            return {};
        }

        return QString::fromNSString(volumeName);
    }

    [[nodiscard]] QString externalVolumeUuidForPath(const QString &path, QString *errorMessage, QString *displayName) const
    {
        if (!externalVolumeStorageAvailable()) {
            if (errorMessage) {
                *errorMessage = FileProviderDomainManager::tr("External File Provider storage requires macOS 15 or later.");
            }
            return {};
        }

        if (@available(macOS 15.0, *)) {
            const auto selectedUrl = [NSURL fileURLWithPath:path.toNSString() isDirectory:YES];
            BOOL eligible = NO;
            NSFileProviderVolumeUnsupportedReason unsupportedReason = NSFileProviderVolumeUnsupportedReasonNone;
            NSError *eligibilityError = nil;

            const auto checkSucceeded = [NSFileProviderManager checkDomainsCanBeStored:&eligible
                                                                         onVolumeAtURL:selectedUrl
                                                                     unsupportedReason:&unsupportedReason
                                                                                 error:&eligibilityError];
            if (!checkSucceeded) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not check File Provider volume eligibility" << eligibilityError.localizedDescription;
                if (errorMessage) {
                    *errorMessage = eligibilityError ? QString::fromNSString(eligibilityError.localizedDescription)
                                                     : FileProviderDomainManager::tr("macOS could not check the selected volume.");
                }
                return {};
            }

            if (!eligible) {
                qCWarning(lcMacFileProviderDomainManager)
                    << "Selected volume is not eligible for File Provider storage; reason flags:" << static_cast<qulonglong>(unsupportedReason);
                if (errorMessage) {
                    *errorMessage = FileProviderDomainManager::tr(
                        "The selected volume cannot store File Provider data. Use an encrypted, writable APFS volume connected directly to this Mac.");
                }
                return {};
            }

            NSString *volumeUuid = nil;
            NSString *volumeName = nil;
            NSError *resourceError = nil;

            if (![selectedUrl getResourceValue:&volumeUuid forKey:NSURLVolumeUUIDStringKey error:&resourceError] || volumeUuid.length == 0) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not read selected volume UUID" << resourceError.localizedDescription;
                if (errorMessage) {
                    *errorMessage = FileProviderDomainManager::tr("macOS did not return an identifier for the selected volume.");
                }
                return {};
            }

            resourceError = nil;
            if (![selectedUrl getResourceValue:&volumeName forKey:NSURLVolumeNameKey error:&resourceError]) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not read selected volume name" << resourceError.localizedDescription;
            }

            if (displayName) {
                *displayName = QString::fromNSString(volumeName);
            }
            if (errorMessage) {
                errorMessage->clear();
            }

            return QString::fromNSString(volumeUuid);
        }

        return {};
    }

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
    QString removeDomain(NSFileProviderDomain *domain, bool *success = nullptr)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing domain"
                                               << domain.identifier;

        dispatch_group_t dispatchGroup = dispatch_group_create();
        dispatch_group_enter(dispatchGroup);

        __block NSURL *preservedDataURL = nil;
        __block bool removeSucceeded = false;

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
            removeSucceeded = true;
            qCInfo(lcMacFileProviderDomainManager) << "Removed domain"
                                                   << domain.identifier;
            dispatch_group_leave(dispatchGroup);
        }];

        dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);

        if (success) {
            *success = removeSucceeded;
        }

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

            if (@available(macOS 15.0, *)) {
                if (domain.volumeUUID != nil) {
                    qCInfo(lcMacFileProviderDomainManager) << "Skipping display-name re-registration for external-volume domain" << domainId;
                    continue;
                }
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
        // Use `<server>[:port] - <user>` ordering so the trailing token is the user. This avoids
        // LaunchServices treating the file provider root folder as a bundle when the server's TLD
        // matches a recognised package extension like ".app" (see issues #7979 / #9684).
        // Domains registered with a stale display name (e.g. the legacy U+2024-escaped form, or
        // a name from before `prettyName()` was updated) are normalised on launch by
        // `reconcileDomainDisplayNames()`.
        const auto domainDisplayName = account->shortcutName();

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

        NSFileProviderDomain *domain = nil;
        auto domainId = QString{};
        std::unique_ptr<Utility::MacSandboxPersistentAccess> volumeAccess;
        const auto configuredVolumeUuid = account->fileProviderDomainVolumeUuid();

        if (configuredVolumeUuid.isEmpty()) {
            domainId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            domain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString() displayName:domainDisplayName.toNSString()];
        } else if (@available(macOS 15.0, *)) {
            volumeAccess = Utility::MacSandboxPersistentAccess::createFromBookmarkData(account->fileProviderDomainVolumeBookmark());
            if (!volumeAccess || !volumeAccess->isValid()) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not access configured File Provider volume for account" << accountId;
                return {};
            }

            const auto volumeUrl = mountedVolumeUrlForUuid(configuredVolumeUuid);
            if (!volumeUrl) {
                qCWarning(lcMacFileProviderDomainManager) << "Configured File Provider volume is not mounted for account" << accountId << configuredVolumeUuid;
                return {};
            }

            QString validationError;
            const auto verifiedVolumeUuid = externalVolumeUuidForPath(QString::fromNSString(volumeUrl.path), &validationError, nullptr);
            if (verifiedVolumeUuid.isEmpty() || verifiedVolumeUuid.compare(configuredVolumeUuid, Qt::CaseInsensitive) != 0) {
                qCWarning(lcMacFileProviderDomainManager)
                    << "Configured File Provider volume is no longer eligible for account" << accountId << validationError;
                return {};
            }

            if (volumeAccess->isStale()) {
                const auto refreshedBookmark = Utility::createSecurityScopedBookmarkData(QString::fromNSString(volumeUrl.path));
                if (refreshedBookmark.isEmpty()) {
                    qCWarning(lcMacFileProviderDomainManager) << "Could not refresh stale File Provider volume bookmark for account" << accountId;
                } else {
                    AccountManager::instance()->setFileProviderDomainVolumeBookmark(accountId, refreshedBookmark);
                    qCInfo(lcMacFileProviderDomainManager) << "Refreshed stale File Provider volume bookmark for account" << accountId;
                }
            }

            const auto userInfo = @{fileProviderAccountIdentifierUserInfoKey : accountId.toNSString()};
            domain = [[NSFileProviderDomain alloc] initWithDisplayName:domainDisplayName.toNSString() userInfo:userInfo volumeURL:volumeUrl];
            domainId = QString::fromNSString(domain.identifier);
        } else {
            qCWarning(lcMacFileProviderDomainManager) << "External File Provider storage requires macOS 15 or later for account" << accountId;
            return {};
        }
        domain.supportsSyncingTrash = YES;

        if (!addDomain(domain)) {
            qCWarning(lcMacFileProviderDomainManager) << "Failed to add file provider domain for account"
                                                      << accountId
                                                      << "- not persisting domain identifier.";
            return {};
        }

        AccountManager::instance()->setFileProviderDomainIdentifier(accountId, domainId);

        return domainId;
    }

    void reconcileExternalDomainMappings()
    {
        if (!externalVolumeStorageAvailable()) {
            return;
        }

        if (@available(macOS 15.0, *)) {
            const auto accountManager = AccountManager::instance();
            const auto domains = getDomains();

            for (NSFileProviderDomain *const domain : domains) {
                if (domain.volumeUUID == nil) {
                    continue;
                }

                const auto accountIdentifierObject = domain.userInfo[fileProviderAccountIdentifierUserInfoKey];
                if (![accountIdentifierObject isKindOfClass:[NSString class]]) {
                    qCWarning(lcMacFileProviderDomainManager) << "External-volume domain has no Nextcloud account mapping" << domain.identifier;
                    continue;
                }

                const auto accountIdentifier = QString::fromNSString((NSString *)accountIdentifierObject);
                const auto accountState = accountManager->accountFromUserId(accountIdentifier);
                if (!accountState || !accountState->account()) {
                    continue;
                }

                const auto account = accountState->account();
                const auto domainVolumeUuid = QString::fromNSString(domain.volumeUUID.UUIDString);
                if (account->fileProviderDomainVolumeUuid().compare(domainVolumeUuid, Qt::CaseInsensitive) != 0) {
                    continue;
                }

                const auto domainIdentifier = QString::fromNSString(domain.identifier);
                if (account->fileProviderDomainIdentifier() != domainIdentifier) {
                    qCInfo(lcMacFileProviderDomainManager)
                        << "Restoring external File Provider domain mapping for account" << accountIdentifier << domainIdentifier;
                    accountManager->setFileProviderDomainIdentifier(accountIdentifier, domainIdentifier);
                }
            }
        }
    }

private:
    id _domainDidChangeObserver = nil;

public:
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

    d->startObservingDomainChanges(this);

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

bool FileProviderDomainManager::externalVolumeStorageAvailable() const
{
    return d && d->externalVolumeStorageAvailable();
}

QString FileProviderDomainManager::externalVolumeUuidForPath(const QString &path, QString *errorMessage, QString *displayName) const
{
    if (!d) {
        if (errorMessage) {
            *errorMessage = tr("File Provider is unavailable.");
        }
        return {};
    }

    return d->externalVolumeUuidForPath(path, errorMessage, displayName);
}

QString FileProviderDomainManager::externalVolumeDisplayNameForUuid(const QString &volumeUuid) const
{
    return d ? d->externalVolumeDisplayNameForUuid(volumeUuid) : QString{};
}

void FileProviderDomainManager::reconcileExternalDomainMappings()
{
    if (d) {
        d->reconcileExternalDomainMappings();
    }
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

    NSFileProviderDomain * const domain = domainForAccount(account.data());

    if (!domain) {
        return {};
    }

    return d->removeDomain(domain);
}

bool FileProviderDomainManager::tryRemoveDomainByAccount(const AccountState *const accountState, QString *preservedDataPath)
{
    if (!d || !accountState || !accountState->account()) {
        return false;
    }

    NSFileProviderDomain *const domain = domainForAccount(accountState->account().data());
    if (!domain) {
        if (preservedDataPath) {
            preservedDataPath->clear();
        }
        return true;
    }

    auto removeSucceeded = false;
    const auto preservedDataUrl = d->removeDomain(domain, &removeSucceeded);
    if (preservedDataPath) {
        *preservedDataPath = preservedDataUrl;
    }
    return removeSucceeded;
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
