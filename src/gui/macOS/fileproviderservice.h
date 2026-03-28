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

private:
    class MacImplementation;
    std::unique_ptr<MacImplementation> d;

    mutable QReadWriteLock _syncStatusLock;
    QHash<QString, SyncResult::Status> _latestReceivedSyncStatus;
};

} // namespace Mac

} // namespace OCC

