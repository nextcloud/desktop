/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QReadWriteLock>

#include "libsync/account.h"
#include "libsync/syncresult.h"

namespace OCC {

namespace Mac {

/**
 * @brief Service that implements the AppProtocol for XPC communication from file provider extensions.
 * 
 * This class provides the implementation of the AppProtocol, allowing file provider extensions
 * to communicate with the main application through XPC. The actual XPC connection management
 * is handled by FileProviderXPCUtils::processClientCommunicationConnections.
 */
class FileProviderService : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderService(QObject *parent = nullptr);
    ~FileProviderService() override;

    /**
     * @brief Get the Objective-C delegate object that implements AppProtocol.
     * @return The delegate pointer (void* to avoid Objective-C in header).
     */
    [[nodiscard]] void *delegate() const;

    /**
     * @brief Get the latest received sync status for a given account.
     * @param account The account for which to get the sync status.
     * @return The latest sync status, or SyncResult::Undefined if not known.
     */
    [[nodiscard]] SyncResult::Status latestReceivedSyncStatusForAccount(const AccountPtr &account) const;

    /**
     * @brief Set the latest received sync status for a user.
     * @param userId The user ID at host with port.
     * @param status The sync status to set.
     */
    void setLatestReceivedSyncStatus(const QString &userId, SyncResult::Status status);

signals:
    /**
     * @brief Emitted when a file provider extension reports its sync status.
     * @param account The account for which the sync state changed.
     * @param state The new sync state.
     */
    void syncStateChanged(const AccountPtr &account, SyncResult::Status state);

    /**
     * @brief Emitted when a file provider extension requests to show the file actions dialog.
     * @param fileId The ocId as provided by the server for item identification independent from path.
     * @param localFile The local file path for which to show actions.
     * @param remoteItemPath The server-side path of the item, used as a fallback when no sync folder is configured.
     * @param fileProviderDomainIdentifier The file provider domain identifier (optional, empty if not provided).
     */
    void showFileActionsDialog(const QString &fileId, const QString &localFile, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier);

    /**
     * @brief Emitted when a file provider extension requests to open an item's page in the user's web browser.
     *
     * The connected slot resolves the per-item private link via `fetchPrivateLinkUrl`
     * and opens the resulting URL via `Utility::openBrowser`, matching the classic-sync
     * "Open in browser" entry. See nextcloud/desktop#10025.
     *
     * @param fileId The **numeric** server file id (WebDAV `fileid`). Not the ocId.
     * @param remoteItemPath The server-side path of the item, used for the PROPFIND that resolves the private link.
     * @param fileProviderDomainIdentifier The file provider domain identifier for the account that owns the item.
     */
    void openItemInBrowserRequested(const QString &fileId, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier);

    /**
     * @brief Emitted when a file provider extension reports an item it refused to sync (for now: macOS bundles).
     *
     * Consumers (e.g. `OCC::User`) surface the item in the systray's activity view — the same
     * place the classic sync engine reports excluded items. See
     * https://github.com/nextcloud/desktop/issues/9827.
     *
     * @param domainIdentifier The file provider domain identifier for the affected account.
     * @param relativePath The path of the item relative to the file provider domain root.
     * @param fileName The display name of the item.
     * @param reason A localized, human-readable explanation of why the item was excluded. Already translated by the extension.
     */
    void itemExcludedFromSync(const QString &domainIdentifier, const QString &relativePath, const QString &fileName, const QString &reason);

    /**
     * @brief Emitted when a file provider extension reports a single item it refused to upload because of insufficient server-side quota.
     *
     * Consumers surface a per-item entry in the activity view — the same shape the classic
     * sync engine produces via `User::slotAddErrorToGui`. See
     * https://github.com/nextcloud/desktop/issues/9598.
     *
     * @param domainIdentifier The file provider domain identifier for the affected account.
     * @param relativePath The path of the item relative to the file provider domain root.
     * @param fileName The display name of the item.
     * @param fileBytes The size of the file the user tried to upload, in bytes. -1 if unknown.
     * @param availableBytes Available quota at the upload's parent at the time of refusal, in bytes. -1 if unknown.
     */
    void insufficientQuotaForItem(const QString &domainIdentifier, const QString &relativePath, const QString &fileName, qint64 fileBytes, qint64 availableBytes);

    /**
     * @brief Emitted when a file provider extension reports that one or more uploads were refused by the server quota for the given domain.
     *
     * Consumers surface a per-folder summary entry in the activity view with a "Retry all
     * uploads" button — the same shape `SyncEngine::slotInsufficientRemoteStorage` →
     * `User::slotAddError(InsufficientRemoteStorage)` produces for classic sync.
     *
     * The extension dedupes the report per domain; consumers do not need to dedupe again.
     *
     * @param domainIdentifier The file provider domain identifier for the affected account.
     */
    void insufficientQuotaSummary(const QString &domainIdentifier);

private:
    class MacImplementation;
    std::unique_ptr<MacImplementation> d;

    mutable QReadWriteLock _syncStatusLock;
    QHash<QString, SyncResult::Status> _latestReceivedSyncStatus;
};

} // namespace Mac

} // namespace OCC

