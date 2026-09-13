/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QList>

#include "accountstate.h"

#ifdef __OBJC__
@class NSFileProviderDomain;
#else
struct NSFileProviderDomain;
#endif

namespace OCC {

class Account;

namespace Mac {

class FileProviderDomainManager : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderDomainManager(QObject * const parent = nullptr);
    ~FileProviderDomainManager() override;

    /**
     * @brief Add a new file provider domain for the given account.
     * @return The raw identifier of the added domain as a string.
     */
    QString addDomainForAccount(const OCC::AccountState * const accountState);

    /** @brief Whether this macOS version provides the File Provider API for storing domains on external volumes. */
    [[nodiscard]] bool externalVolumeStorageAvailable() const;

    /**
     * @brief Validate a mounted volume for File Provider storage and return its UUID.
     * @param path Path to the root of the mounted volume.
     * @param errorMessage Optional destination for a user-visible validation error.
     * @param displayName Optional destination for the mounted volume's display name.
     * @return The volume UUID, or an empty string when the volume cannot be used.
     */
    [[nodiscard]] QString externalVolumeUuidForPath(const QString &path, QString *errorMessage = nullptr, QString *displayName = nullptr) const;

    /** @brief Return the display name of a currently mounted volume identified by UUID, or an empty string when it is unavailable. */
    [[nodiscard]] QString externalVolumeDisplayNameForUuid(const QString &volumeUuid) const;

    /** @brief Restore account-to-domain identifiers for registered external-volume domains whose identifiers changed. */
    void reconcileExternalDomainMappings();

    /**
     * @brief Remove all file provider domains managed by this application.
     */
    void removeAllDomains();

    /**
     * @brief Reconnect all file provider domains.
     */
    void reconnectAll();

    /**
     * @brief Reconcile registered file provider domain display names with the current account state.
     *
     * For every registered domain whose `displayName` no longer matches the owning account's
     * `Account::shortcutName()` (e.g. the legacy U+2024-escaped form left over from the prior
     * fix for #7979, or a stale `prettyName()` snapshot), re-register the domain in place so the
     * macOS-side metadata catches up. Safe to call on every launch — a no-op when names already
     * match.
     */
    void reconcileDomainDisplayNames();

    /**
     * @brief Remove a file provider domain independent from an account.
     * @return The path to the location where preserved dirty user data is stored, or an empty QString if none.
     */
    QString removeDomain(NSFileProviderDomain *domain);

    /**
     * @brief Remove the file provider domain for the given account.
     * @return The path to the location where preserved dirty user data is stored, or an empty QString if none.
     */
    QString removeDomainByAccount(const OCC::AccountState * const accountState);

    /**
     * @brief Remove the file provider domain for an account and report whether macOS completed the removal.
     * @param preservedDataPath Receives the path to preserved dirty user data when macOS provides one.
     * @return true when the domain was removed or was already absent; false when removal failed.
     */
    bool tryRemoveDomainByAccount(const OCC::AccountState *const accountState, QString *preservedDataPath = nullptr);

    void start();

    [[nodiscard]] QList<NSFileProviderDomain *> getDomains() const;

    NSFileProviderDomain *domainForAccount(const OCC::Account *account) const;

    void signalEnumeratorChanged(const OCC::Account * const account);

    /**
     * @brief Get the user-visible URL for a file provider domain's root container.
     * @param domainIdentifier The identifier of the file provider domain.
     * @return The user-visible URL of the domain's root container, or an empty QString if not found.
     */
    [[nodiscard]] QString userVisibleUrlForDomainIdentifier(const QString &domainIdentifier) const;

    /**
     * @brief Open the system file viewer at the root container of a file provider domain.
     * @param domainIdentifier The identifier of the file provider domain.
     */
    void openFileViewerForDomainIdentifier(const QString &domainIdentifier) const;

    /**
     * @brief Tell the system that the previously-returned `NSFileProviderError.insufficientQuota` error no longer applies and signal an enumeration of the working set so the system retries refused uploads.
     *
     * Implements the documented contract of
     * `NSFileProviderManager.signalErrorResolved(_:completionHandler:)` followed by
     * `signalEnumerator(for: .workingSet)`. Called from `User::slotFileProviderRetryUploads`
     * when the user clicks the "Retry all uploads" button on the per-folder summary entry
     * surfaced for nextcloud/desktop#9598.
     *
     * @param domainIdentifier The identifier of the affected file provider domain.
     */
    void clearInsufficientQuotaErrorAndEnumerate(const QString &domainIdentifier) const;

Q_SIGNALS:
    void domainsChanged();

public Q_SLOTS:
    /**
     * @brief Handle file ID changes from push notifications
     * @param account The account for which file IDs changed
     * @param fileIds List of file IDs that have changed
     */
    void slotHandleFileIdsChanged(const OCC::Account *account, const QList<qint64> &fileIds);

private Q_SLOTS:
    void disconnectFileProviderDomainForAccount(const OCC::AccountState * const accountState, const QString &reason);
    void reconnectFileProviderDomainForAccount(const OCC::AccountState * const accountState);
    void slotAccountStateChanged(const OCC::AccountState * const accountState);

private:
    class MacImplementation;
    std::unique_ptr<MacImplementation> d;
};

} // namespace Mac

} // namespace OCC
