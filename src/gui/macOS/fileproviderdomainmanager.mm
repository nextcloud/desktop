/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <AppKit/AppKit.h>
#import <FileProvider/FileProvider.h>

#include <algorithm>
#include <memory>

#include <QDir>
#include <QHash>
#include <QLatin1StringView>
#include <QList>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>
#include <QUuid>

#include "config.h"
#include "fileprovider.h"
#include "fileproviderdomainmanager.h"
#include "fileproviderdomainmapping.h"
#include "fileproviderexternalaccountregistry.h"
#include "fileprovidersettingscontroller.h"
#include "fileproviderutils.h"
#include "fileproviderutils_mac.h"
#include "fileproviderxpc_mac_utils.h"

#include "common/utility_mac_sandbox.h"
#include "gui/accountmanager.h"
#include "libsync/account.h"

static_assert(__has_feature(objc_arc), "fileproviderdomainmanager requires ARC.");

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

        _domainDidChangeObserver =
            [[NSNotificationCenter defaultCenter] addObserverForName:NSFileProviderDomainDidChange
                                                              object:nil
                                                               queue:[NSOperationQueue mainQueue]
                                                          usingBlock:^(__unused NSNotification *notification) { scheduleDomainChangeReconciliation(manager); }];
    }

    void scheduleDomainChangeReconciliation(FileProviderDomainManager *manager)
    {
        if (_domainChangeReconciliationScheduled) {
            return;
        }

        _domainChangeReconciliationScheduled = true;
        const QPointer<FileProviderDomainManager> managerGuard(manager);
        QTimer::singleShot(100, manager, [this, managerGuard] {
            if (!managerGuard) {
                return;
            }

            if (FileProviderSettingsController::instance()->isOperationInProgress()) {
                _domainChangeReconciliationScheduled = false;
                scheduleDomainChangeReconciliation(managerGuard.data());
                return;
            }

            [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error) {
                if (!managerGuard) {
                    return;
                }

                if (error) {
                    qCWarning(lcMacFileProviderDomainManager)
                        << "Could not reconcile File Provider domain-list change because domain enumeration failed" << error.code << error.localizedDescription;
                    QMetaObject::invokeMethod(
                        managerGuard.data(),
                        [this, managerGuard] {
                            if (managerGuard) {
                                _domainChangeReconciliationScheduled = false;
                            }
                        },
                        Qt::QueuedConnection);
                    return;
                }

                NSArray<NSFileProviderDomain *> *const retainedDomains = [domains copy];
                QMetaObject::invokeMethod(
                    managerGuard.data(),
                    [this, managerGuard, retainedDomains] {
                        if (!managerGuard) {
                            return;
                        }

                        _domainChangeReconciliationScheduled = false;

                        if (FileProviderSettingsController::instance()->isOperationInProgress()) {
                            scheduleDomainChangeReconciliation(managerGuard.data());
                            return;
                        }

                        QList<NSFileProviderDomain *> domainList;
                        QSet<QString> domainIdentifiers;
                        domainList.reserve(retainedDomains.count);
                        for (NSFileProviderDomain *const domain in retainedDomains) {
                            domainList.append(domain);
                            domainIdentifiers.insert(QString::fromNSString(domain.identifier));
                        }

                        const auto domainIdentifiersChanged = domainIdentifiers != _observedDomainIdentifiers;
                        _observedDomainIdentifiers = domainIdentifiers;

                        qCInfo(lcMacFileProviderDomainManager) << "Reconciling File Provider domain-list change";
                        (void)reconcileExternalDomainMappings(domainList);
                        Q_EMIT managerGuard->domainsChanged();

                        if (domainIdentifiersChanged) {
                            FileProvider::instance()->configureXPC();
                        }
                    },
                    Qt::QueuedConnection);
            }];
        });
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
     * @return Whether the system confirmed that the domain was added before the wait ended.
     */
    bool addDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Adding domain" << domain.identifier;
        return FileProviderUtils::waitForFileProviderRequest("adding domain", ^(FileProviderUtils::RequestReplyHandler reply) {
            [NSFileProviderManager addDomain:domain completionHandler:^(NSError *error) { reply(nil, error); }];
        });
    }

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager disconnectWithReason:]`.
     */
    void disconnect(NSFileProviderDomain *domain, const QString &message)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Disconnecting domain" << domain.identifier;
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return;
        }

        NSString *const reason = message.toNSString();
        FileProviderUtils::waitForFileProviderRequest("disconnecting domain", ^(FileProviderUtils::RequestReplyHandler reply) {
            [manager disconnectWithReason:reason
                                  options:NSFileProviderManagerDisconnectionOptionsTemporary
                        completionHandler:^(NSError *error) { reply(nil, error); }];
        });
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
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect domain" << domain.identifier;
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return;
        }

        const auto reconnected = FileProviderUtils::waitForFileProviderRequest("reconnecting domain", ^(FileProviderUtils::RequestReplyHandler reply) {
            [manager reconnectWithCompletionHandler:^(NSError *error) { reply(nil, error); }];
        });
        if (reconnected) {
            signalEnumerator(domain);
        }
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
     * @brief Remove a domain and its local support data after confirmed success.
     * @return The preserved-data path, empty for no data, or nullopt if removal was not confirmed.
     */
    std::optional<QString> removeDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing domain" << domain.identifier;
        deletePassword(domain);

        id result = nil;
        const auto removed = FileProviderUtils::waitForFileProviderRequest(
            "removing domain",
            ^(FileProviderUtils::RequestReplyHandler reply) {
                [NSFileProviderManager removeDomain:domain
                                               mode:NSFileProviderDomainRemovalModePreserveDirtyUserData
                                  completionHandler:^(NSURL *dataURL, NSError *error) {
                                      if (!reply(dataURL, error) && !error && dataURL) {
                                          qCWarning(lcMacFileProviderDomainManager) << "Late domain removal preserved data at:" << dataURL.path;
                                      }
                                  }];
            },
            &result);
        if (!removed) {
            return std::nullopt;
        }

        NSURL *const preservedDataURL = result;
        if (preservedDataURL) {
            qCInfo(lcMacFileProviderDomainManager) << "Domain removed with preserved data at:" << preservedDataURL.path;
        }
        removeFileProviderDomainData(domain.identifier);
        qCInfo(lcMacFileProviderDomainManager) << "Removed domain" << domain.identifier;
        return QString::fromNSString(preservedDataURL.path);
    }

    void signalEnumerator(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Signaling enumerator for domain" << domain.identifier;
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return;
        }

        FileProviderUtils::waitForFileProviderRequest("signalling enumerator", ^(FileProviderUtils::RequestReplyHandler reply) {
            [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier
                                              completionHandler:^(NSError *error) { reply(nil, error); }];
        });
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
    bool removeAllDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all domains...";
        auto success = false;
        const auto domains = getDomains(&success);
        if (!success) {
            return false;
        }
        for (NSFileProviderDomain * const domain : domains) {
            if (@available(macOS 15.0, *)) {
                if (domain.volumeUUID != nil) {
                    continue;
                }
            }
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

        reconcileExternalDomainMappings(domains);
        const auto restoredDomainId = account->fileProviderDomainIdentifier();
        for (NSFileProviderDomain *const domain : domains) {
            if (!restoredDomainId.isEmpty() && restoredDomainId == QString::fromNSString(domain.identifier)) {
                _pendingAddIdentifiers.remove(accountId);
                return restoredDomainId;
            }
        }

        NSFileProviderDomain *domain = nil;
        auto domainId = QString{};
        std::unique_ptr<Utility::MacSandboxPersistentAccess> volumeAccess;
        const auto configuredVolumeUuid = account->fileProviderDomainVolumeUuid();

        if (configuredVolumeUuid.isEmpty()) {
            domainId = pendingDomainId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : pendingDomainId;
            domain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString() displayName:domainDisplayName.toNSString()];
        } else if (@available(macOS 15.0, *)) {
            const auto volumeUrl = mountedVolumeUrlForUuid(configuredVolumeUuid);
            if (!volumeUrl) {
                qCWarning(lcMacFileProviderDomainManager) << "Configured File Provider volume is not mounted for account" << accountId << configuredVolumeUuid;
                return {};
            }

            volumeAccess = Utility::MacSandboxPersistentAccess::createFromBookmarkData(account->fileProviderDomainVolumeBookmark());
            if (!volumeAccess || !volumeAccess->isValid()) {
                qCWarning(lcMacFileProviderDomainManager) << "Could not access configured File Provider volume for account" << accountId;
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
                    AccountManager::instance()->setFileProviderDomainStorage(accountId, configuredVolumeUuid, refreshedBookmark);
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
        _pendingAddIdentifiers.insert(accountId, domainId);
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

    bool reconcileExternalDomainMappings()
    {
        return reconcileExternalDomainMappings(getDomains());
    }

    bool reconcileExternalDomainMappings(const QList<NSFileProviderDomain *> &domains)
    {
        if (!externalVolumeStorageAvailable() || !ConfigFile().macFileProviderModeEnabled()) {
            return false;
        }

        if (@available(macOS 15.0, *)) {
            const auto accountManager = AccountManager::instance();
            QList<FileProviderDomainMapping::DomainDescriptor> descriptors;
            descriptors.reserve(domains.size());

            for (NSFileProviderDomain *const domain : domains) {
                auto accountIdentifier = QString{};
                if (domain.volumeUUID != nil) {
                    NSObject *const accountIdentifierObject = domain.userInfo[fileProviderAccountIdentifierUserInfoKey];
                    if ([accountIdentifierObject isKindOfClass:[NSString class]]) {
                        accountIdentifier = QString::fromNSString((NSString *)accountIdentifierObject);
                    }
                }

                descriptors.append({
                    QString::fromNSString(domain.identifier),
                    accountIdentifier,
                    domain.volumeUUID == nil ? QString{} : QString::fromNSString(domain.volumeUUID.UUIDString),
                    domain.disconnected,
                });
            }

            auto requiresXpcReconfiguration = false;
            for (const auto &accountState : accountManager->accounts()) {
                const auto account = accountState->account();
                if (!account || account->fileProviderDomainVolumeUuid().isEmpty()) {
                    continue;
                }

                const auto accountIdentifier = account->userIdAtHostWithPort();
                const auto domainIdentifier = FileProviderDomainMapping::domainIdentifierToAdopt(accountIdentifier,
                                                                                                 account->fileProviderDomainVolumeUuid(),
                                                                                                 account->fileProviderDomainIdentifier(),
                                                                                                 descriptors);

                auto effectiveDomainIdentifier = account->fileProviderDomainIdentifier();
                if (!domainIdentifier.isEmpty()) {
                    qCInfo(lcMacFileProviderDomainManager)
                        << "Restoring external File Provider domain mapping for account" << accountIdentifier << domainIdentifier;
                    accountManager->setFileProviderDomainIdentifier(accountIdentifier, domainIdentifier);
                    effectiveDomainIdentifier = domainIdentifier;
                    requiresXpcReconfiguration = true;
                }

                const auto reconnectIdentifier = FileProviderDomainMapping::domainIdentifierToReconnect(effectiveDomainIdentifier,
                                                                                                        accountState->state() == AccountState::Connected,
                                                                                                        descriptors);
                if (reconnectIdentifier.isEmpty()) {
                    continue;
                }

                const auto domainIt = std::find_if(domains.cbegin(), domains.cend(), [&](NSFileProviderDomain *domain) {
                    return QString::fromNSString(domain.identifier) == reconnectIdentifier;
                });
                if (domainIt == domains.cend()) {
                    continue;
                }

                qCInfo(lcMacFileProviderDomainManager)
                    << "Reconnecting external File Provider domain for connected account" << accountIdentifier << reconnectIdentifier;
                reconnect(*domainIt);
                requiresXpcReconfiguration = true;
            }

            return requiresXpcReconfiguration;
        }

        return false;
    }

private:
    id _domainDidChangeObserver = nil;
    bool _domainChangeReconciliationScheduled = false;
    QSet<QString> _observedDomainIdentifiers;

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

    updateExternalDomainConnectionEligibility();
    d->startObservingDomainChanges(this);

    ConfigFile cfg;

    // If an account is deleted from the client, accountSyncConnectionRemoved will be
    // emitted first. So we treat accountRemoved as only being relevant to client
    // shutdowns.
    connect(AccountManager::instance(), &AccountManager::accountSyncConnectionRemoved, this, [this](const AccountState *const accountState) {
        removeDomainByAccount(accountState);
        QTimer::singleShot(0, this, [this] {
            updateExternalDomainConnectionEligibility();
        });
    });

    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, [this](const AccountState * const accountState) {
        const auto trReason = tr("%1 application has been closed. Reopen to reconnect.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
        QTimer::singleShot(0, this, [this] {
            updateExternalDomainConnectionEligibility();
        });
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

void FileProviderDomainManager::updateExternalDomainConnectionEligibility()
{
    QList<QPair<QString, QString>> accountStorage;

    if (ConfigFile().macFileProviderModeEnabled()) {
        const auto accountStates = AccountManager::instance()->accounts();
        accountStorage.reserve(accountStates.size());
        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();
            if (account) {
                accountStorage.append({account->userIdAtHostWithPort(), account->fileProviderDomainVolumeUuid()});
            }
        }
    }

    FileProviderUtils::setExternalVolumeAccountIdentifiers(FileProviderExternalAccountRegistry::configuredAccountIdentifiers(accountStorage));
}

bool FileProviderDomainManager::reconcileExternalDomainMappings()
{
    return d && d->reconcileExternalDomainMappings();
}

bool FileProviderDomainManager::removeAllDomains()
{
    return d && d->removeAllDomains();
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

std::optional<QString> FileProviderDomainManager::removeDomain(NSFileProviderDomain *domain)
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

    updateExternalDomainConnectionEligibility();

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

std::optional<QString> FileProviderDomainManager::removeDomainByAccount(const AccountState *const accountState)
{
    if (!d || !accountState) {
        return std::nullopt;
    }
    const auto account = accountState->account();

    if (!account) {
        return {};
    }

    if (account->fileProviderDomainIdentifier().isEmpty()) {
        return QString{};
    }

    auto success = false;
    const auto domains = d->getDomains(&success);
    if (!success) {
        return std::nullopt;
    }
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
