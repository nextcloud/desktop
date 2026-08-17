//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import RealmSwift

public extension Item {
    /// > Note: The trashing parameter does not affect whether the server will trash this or not.
    /// That's out of our hands. Instead, this is used internally to properly handle the metadata
    /// update automatically when we conduct a move of an item to the trash.
    func delete(
        trashing: Bool = false,
        options: NSFileProviderDeleteItemOptions = [.recursive],
        domain: NSFileProviderDomain? = nil,
        ignoredFiles: IgnoredFilesMatcher? = nil,
        dbManager: FilesDatabaseManager
    ) async -> Error? {
        let isEmptyDirOrIsFile = childItemCount == nil || childItemCount == 0

        guard trashing || isEmptyDirOrIsFile || options.contains(.recursive) else {
            return NSFileProviderError(.directoryNotEmpty)
        }

        let chunkUploadOwnerIdentifiersToDiscard = chunkUploadItemIdentifiersToDiscard()
        var deletionCompleted = false
        defer {
            if deletionCompleted {
                discardChunkUploads(
                    forItemIdentifiers: chunkUploadOwnerIdentifiersToDiscard,
                    usingRemoteInterface: remoteInterface,
                    dbManager: dbManager,
                    logger: logger
                )
            }
        }

        let ocId = itemIdentifier.rawValue
        let relativePath = (metadata.remotePath()).replacingOccurrences(of: metadata.urlBase, with: "")

        guard metadata.isLockFileOfLocalOrigin == false else {
            return await deleteLockFile(domain: domain, dbManager: dbManager)
        }

        if dbManager.isItemExcludedFromSync(ocId: ocId) {
            logger.info("Item deletion follows an exclusion from sync. Will delete from local database with no remote effect.", [.item: itemIdentifier, .name: filename])

            guard handleMetadataDeletion() else {
                return NSFileProviderError(.cannotSynchronize)
            }

            guard dbManager.removeExcludedFromSyncMarker(ocId: ocId) else {
                return NSFileProviderError(.cannotSynchronize)
            }

            deletionCompleted = true
            return nil
        }

        guard ignoredFiles == nil || ignoredFiles?.isExcluded(relativePath) == false else {
            logger.info("File is in the ignore list. Will delete from local database with no remote effect.", [.item: itemIdentifier, .name: filename])
            deletionCompleted = true
            dbManager.deleteItemMetadata(ocId: ocId)
            return nil
        }

        var serverFileNameUrl = metadata.remotePath()

        guard serverFileNameUrl != "" else {
            return NSError.fileProviderErrorForNonExistentItem(withIdentifier: itemIdentifier)
        }

        guard metadata.classFile != "lock", !isLockFileName(metadata.fileName) else {
            return await deleteLockFile(domain: domain, dbManager: dbManager)
        }

        // Permanently deleting an item that already lives in the trash. The stored trashbin name can be
        // the "rough" plain filename set optimistically when the item was moved to trash (see
        // handleMetadataTrashModification below), rather than the server's real trashbin name — which
        // carries a ".d<deletion-timestamp>" suffix whenever the name collided with an existing trash
        // entry. DELETEing the plain path 404s forever, and macOS retries the purge with no backoff
        // (observed: a handful of items failing thousands of times). Resolve the real trashbin name from
        // a fresh trash listing; if the item is no longer in the trash, it has already been removed
        // remotely, so report success and stop the retry loop. See nextcloud/desktop#10442.
        let isTrashbinPurge = !trashing && metadata.serverUrl.hasPrefix(account.trashUrl)
        if isTrashbinPurge {
            switch await resolveTrashbinItemRemotePath(domain: domain) {
                case let .resolved(resolvedUrl):
                    serverFileNameUrl = resolvedUrl
                case .alreadyGone:
                    logger.info(
                        "Trashbin item no longer present in a fresh trash listing; treating permanent delete as already complete.",
                        [.item: ocId, .name: filename]
                    )
                    deletionCompleted = true
                    handleMetadataDeletion()
                    return nil
                case .unresolved:
                    logger.info(
                        "Could not resolve the trashbin item's server name from a fresh listing; falling back to the stored path.",
                        [.item: ocId, .url: serverFileNameUrl]
                    )
            }
        }

        let (_, _, error) = await remoteInterface.delete(
            remotePath: serverFileNameUrl,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard error == .success else {
            // A purge that 404s means the item is already gone from the trash — the desired end state.
            // Report success so macOS stops re-issuing the (now pointless) permanent delete.
            if isTrashbinPurge, error.isNotFoundError {
                logger.info(
                    "Trashbin item returned 404 on permanent delete; it is already gone, treating as complete.",
                    [.item: ocId, .url: serverFileNameUrl]
                )
                deletionCompleted = true
                handleMetadataDeletion()
                return nil
            }
            logger.error("Could not delete item.", [.item: ocId, .url: serverFileNameUrl, .error: error])
            return error.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: itemIdentifier)
        }

        logger.info("Successfully deleted item.", [.item: ocId, .url: serverFileNameUrl])
        deletionCompleted = true

        guard trashing else {
            handleMetadataDeletion()
            return nil
        }

        return handleMetadataTrashModification()
    }

    private func chunkUploadItemIdentifiersToDiscard() -> [String] {
        guard metadata.directory else {
            return [metadata.ocId]
        }

        let directoryRemotePath = metadata.remotePath()
        let itemAccount = metadata.account
        return dbManager.itemMetadatas
            .where {
                $0.directory == false &&
                    $0.account == itemAccount &&
                    // Keep chunks for in-progress or failed uploads that recursive metadata deletion
                    // deliberately preserves.
                    $0.status < Status.inUpload.rawValue &&
                    RealmItemMetadata.hasServerUrl(
                        $0,
                        equalTo: directoryRemotePath,
                        includingDescendants: true
                    )
            }
            .map(\.ocId)
    }

    @discardableResult
    private func handleMetadataDeletion() -> Bool {
        let ocId = metadata.ocId

        if metadata.directory {
            return dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId) != nil
        }

        return dbManager.deleteItemMetadata(ocId: ocId)
    }

    /// NOTE: the trashing metadata modification procedure here is rough. You SHOULD run a rescan of
    /// the trash in order to ensure you are getting a correct picture of the item's current remote
    /// state! This is important particularly for receiving the correct trash bin filename in case of
    /// there being a previous item in the trash with the same name, prompting the server to rename
    /// the newly-trashed target item
    private func handleMetadataTrashModification() -> Error? {
        let ocId = metadata.ocId

        if metadata.directory {
            _ = dbManager.renameDirectoryAndPropagateToChildren(
                ocId: ocId,
                newServerUrl: account.trashUrl,
                newFileName: filename
            )
        } else {
            dbManager.renameItemMetadata(ocId: ocId, newServerUrl: account.trashUrl, newFileName: filename)
        }

        guard var metadata = dbManager.itemMetadata(ocId: ocId) else {
            logger.error("Could not find item metadata! Cannot finish trashing procedure!", [.item: itemIdentifier, .name: filename])
            return NSFileProviderError(.cannotSynchronize)
        }

        metadata.trashbinFileName = filename
        metadata.trashbinDeletionTime = Date()
        metadata.trashbinOriginalLocation = String(self.metadata.serverUrl + "/" + filename).replacingOccurrences(of: account.davFilesUrl + "/", with: "")
        dbManager.addItemMetadata(metadata)

        return nil
    }

    /// Outcome of resolving the true server path for a trashbin item about to be permanently deleted.
    enum TrashbinItemRemotePathResolution: Sendable {
        /// The item was found in the remote trash; the associated URL is the correct DELETE target
        /// (including any server-assigned ".d<deletion-timestamp>" suffix).
        case resolved(String)
        /// The item is no longer present in the remote trash — already permanently removed.
        case alreadyGone
        /// The trash listing itself failed (e.g. transient error); the caller should fall back.
        case unresolved
    }

    /// Look up the item's current entry in the remote trash and return the correct DELETE URL.
    ///
    /// The database may hold a "rough" plain trashbin filename (see ``handleMetadataTrashModification()``),
    /// but the server names collided trash entries `"<name>.d<deletion-timestamp>"`. Matching the fresh
    /// listing by `ocId`/`fileId` (the server sometimes returns the `fileId` as the trash `ocId`, mirroring
    /// ``Item/trash(_:account:dbManager:domain:log:)``) yields the real on-server name.
    private func resolveTrashbinItemRemotePath(
        domain: NSFileProviderDomain?
    ) async -> TrashbinItemRemotePathResolution {
        let (_, items, _, error) = await remoteInterface.listingTrashAsync(
            filename: nil,
            showHiddenFiles: true,
            account: account.ncKitAccount,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )

        guard error == .success else {
            return .unresolved
        }

        guard let match = items?.first(where: {
            $0.ocId == metadata.ocId || $0.fileId == metadata.fileId
        }) else {
            return .alreadyGone
        }

        return .resolved(account.trashUrl + "/" + match.fileName)
    }
}
