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

    ///
    /// Full enumeration of the trash container.
    ///
    /// The trash only ever lists items deleted on this device, which cannot exist before the initial
    /// content enumeration of a domain. The initial trash enumeration therefore finishes with an
    /// empty set once trash support has been confirmed in the server capabilities.
    ///
    func enumerateTrashItems(for observer: NSFileProviderEnumerationObserver) {
        logger.info("Enumerating items in trash.", [.account: account.ncKitAccount, .url: serverUrl])

        Task { [weak self] in
            guard let self else {
                return
            }

            let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

            guard let capabilities, error == .success else {
                logger.error("Could not acquire capabilities, cannot check trash.", [.error: error])
                observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                return
            }

            guard capabilities.files?.undelete == true else {
                logger.error("Trash is unsupported on server, cannot enumerate items.")
                observer.finishEnumeratingWithError(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError))
                return
            }

            // We only want to list items deleted on the local device.
            // That cannot happen before the initial content enumeration for a file provider domain because the latter does not exist yet.
            // Hence the initial trash content enumeration can be finished with an empty set.
            observer.finishEnumerating(upTo: nil)
        }
    }

    ///
    /// Change enumeration of the trash container: list the remote trash and let
    /// ``completeChangesObserver(_:anchor:account:dbManager:remoteTrashItems:log:)`` reconcile it
    /// against the local trash to report permanently-deleted items.
    ///
    func enumerateTrashChanges(for observer: NSFileProviderChangeObserver, anchor: NSFileProviderSyncAnchor) {
        logger.debug("Enumerating changes in trash.", [.account: account.ncKitAccount])

        Task { [weak self] in
            guard let self else {
                return
            }

            let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

            guard let capabilities, error == .success else {
                logger.error("Could not acquire capabilities, cannot check trash.", [.error: error])
                observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
                return
            }

            guard capabilities.files?.undelete == true else {
                logger.error("Trash is unsupported on server. Cannot enumerate changes.")

                observer.finishEnumeratingWithError(
                    NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
                )
                return
            }

            let domain = domain
            let enumeratedItemIdentifier = enumeratedItemIdentifier

            let (_, trashedItems, _, trashReadError) = await remoteInterface.listingTrashAsync(
                filename: nil,
                showHiddenFiles: true,
                account: account.ncKitAccount,
                options: .init(),
                taskHandler: { task in
                    if let domain {
                        NSFileProviderManager(for: domain)?.register(
                            task,
                            forItemWithIdentifier: enumeratedItemIdentifier,
                            completionHandler: { _ in }
                        )
                    }
                }
            )

            guard trashReadError == .success else {
                let error = trashReadError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier) ?? NSFileProviderError(.cannotSynchronize)
                observer.finishEnumeratingWithError(error)
                return
            }

            await Self.completeChangesObserver(
                observer,
                anchor: anchor,
                account: account,
                dbManager: dbManager,
                remoteTrashItems: trashedItems ?? [],
                log: logger.log
            )
        }
    }
}
