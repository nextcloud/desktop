//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudKit

extension Enumerator {
    static func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        numPage: Int,
        trashItems: [NKTrash]
    ) {
        var metadatas = [SendableItemMetadata]()
        for trashItem in trashItems {
            let metadata = trashItem.toItemMetadata(account: account)
            dbManager.addItemMetadata(metadata)
            metadatas.append(metadata)
        }

        Task { [metadatas] in
            do {
                let items = try await metadatas.toFileProviderItems(
                    account: account, remoteInterface: remoteInterface, dbManager: dbManager
                )
                Task { @MainActor in
                    observer.didEnumerate(items)
                    Self.logger.info("Did enumerate \(items.count) trash items")
                    observer.finishEnumerating(upTo: fileProviderPageforNumPage(numPage))
                }
            } catch let error {
                Self.logger.info("Unexpected error enumerating trash items, observing error.")
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
        trashItems: [NKTrash]
    ) async {
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

            let item = Item(
                metadata: metadata,
                parentItemIdentifier: .trashContainer,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: await remoteInterface.supportsTrash(account: account)
            )
            newTrashedItems.append(item)

            Self.logger.debug(
                """
                Will enumerate changed trashed item with ocId: \(metadata.ocId, privacy: .public)
                and name: \(metadata.fileName, privacy: .public)
                """
            )
        }

        let deletedTrashedItemsIdentifiers = existingTrashedItems.map {
            NSFileProviderItemIdentifier($0.ocId)
        }
        if !deletedTrashedItemsIdentifiers.isEmpty {
            for itemIdentifier in deletedTrashedItemsIdentifiers {
                dbManager.deleteItemMetadata(ocId: itemIdentifier.rawValue)
            }
            Self.logger.debug(
                """
                Will enumerate deleted trashed items:
                \(deletedTrashedItemsIdentifiers, privacy: .public)
                """
            )
            observer.didDeleteItems(withIdentifiers: deletedTrashedItemsIdentifiers)
        }

        if !newTrashedItems.isEmpty {
            observer.didUpdate(newTrashedItems)
        }
        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
    }
}
