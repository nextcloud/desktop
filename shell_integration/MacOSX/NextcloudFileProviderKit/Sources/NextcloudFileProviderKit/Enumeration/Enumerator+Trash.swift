//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Enumerator {
    ///
    /// Change enumeration completion.
    ///
    /// `NKTrash` items do not have an ETag. We assume they cannot be modified while they are in the trash. So we will just check by their `ocId`.
    /// Newly added items by deletion on the server side or another client are not of interest and we do not want to display them in the local trash.
    /// In the end, only the remotely and permanently deleted items are of interest.
    ///
    static func completeChangesObserver(_ observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor, account: Account, dbManager: FilesDatabaseManager, remoteTrashItems: [NKTrash], log: any FileProviderLogging) async {
        let logger = FileProviderLogger(category: "Enumerator", log: log)
        let localIdentifiers = dbManager.trashedItemMetadatas(account: account).map(\.ocId)
        let localSet = Set(localIdentifiers)
        let remoteIdentifiers = remoteTrashItems.map(\.ocId)
        let remoteSet = Set(remoteIdentifiers)
        let orphanedSet = localSet.subtracting(remoteSet)
        let orphanedIdentifiers = orphanedSet.map { NSFileProviderItemIdentifier($0) }

        for identifier in orphanedSet {
            logger.info("Permanently deleting remote trash item which could not be matched with a local one.", [.item: identifier])
            dbManager.deleteItemMetadata(ocId: identifier)
        }

        observer.didDeleteItems(withIdentifiers: orphanedIdentifiers)
        observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)
        logger.debug("Finished enumerating remote changes in trash.")
    }
}
