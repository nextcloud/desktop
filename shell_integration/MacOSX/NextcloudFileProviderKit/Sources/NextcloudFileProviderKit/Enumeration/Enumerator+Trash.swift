//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Enumerator {
    static func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        numPage: Int,
        trashItems: [NKTrash],
        log: any FileProviderLogging
    ) {
        var metadatas = [SendableItemMetadata]()
        for trashItem in trashItems {
            let metadata = trashItem.toItemMetadata(account: account)
            dbManager.addItemMetadata(metadata)
            metadatas.append(metadata)
        }

        Task { [metadatas] in
            let logger = FileProviderLogger(category: "Enumerator", log: log)

            do {
                let items = try await metadatas.toFileProviderItems(account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: log)

                Task { @MainActor in
                    observer.didEnumerate(items)
                    logger.info("Did enumerate \(items.count) trash items.")
                    observer.finishEnumerating(upTo: fileProviderPageforNumPage(numPage))
                }
            } catch {
                logger.error("Finishing enumeration with error.")
                Task { @MainActor in observer.finishEnumeratingWithError(error) }
            }
        }
    }

    static func completeChangesObserver(
        _ observer: NSFileProviderChangeObserver,
        anchor: NSFileProviderSyncAnchor,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        trashItems: [NKTrash],
        log: any FileProviderLogging
    ) async {
        let logger = FileProviderLogger(category: "Enumerator", log: log)
        var newTrashedItems = [NSFileProviderItem]()

        // NKTrash items do not have an etag ; we assume they cannot be modified while they are in
        // the trash, so we will just check by ocId
        var existingTrashedItems = dbManager.trashedItemMetadatas(account: account)

        for trashItem in trashItems {
            if let existingTrashItemIndex = existingTrashedItems.firstIndex(
                where: { $0.ocId == trashItem.ocId }
            ) {
                existingTrashedItems.remove(at: existingTrashItemIndex)
                continue
            }

            let metadata = trashItem.toItemMetadata(account: account)
            dbManager.addItemMetadata(metadata)

            let item = await Item(
                metadata: metadata,
                parentItemIdentifier: .trashContainer,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
                log: log
            )
            newTrashedItems.append(item)

            logger.debug("Will enumerate changed trash item.", [.item: metadata.ocId, .name: metadata.fileName])
        }

        let deletedTrashedItemsIdentifiers = existingTrashedItems.map {
            NSFileProviderItemIdentifier($0.ocId)
        }
        if !deletedTrashedItemsIdentifiers.isEmpty {
            for itemIdentifier in deletedTrashedItemsIdentifiers {
                dbManager.deleteItemMetadata(ocId: itemIdentifier.rawValue)
            }

            logger.debug("Will enumerate deleted trashed items: \(deletedTrashedItemsIdentifiers)")
            observer.didDeleteItems(withIdentifiers: deletedTrashedItemsIdentifiers)
        }

        if !newTrashedItems.isEmpty {
            observer.didUpdate(newTrashedItems)
        }
        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
    }
}
