//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

///
/// Translating derived metadata into the `NSFileProviderEnumerationObserver` /
/// `NSFileProviderChangeObserver` callbacks the framework expects, plus the shared recovery used
/// when an item's parent identifier is not yet known locally.
///
extension Enumerator {
    func completeEnumerationObserver(
        _ observer: NSFileProviderEnumerationObserver,
        nextPage: EnumeratorPageResponse?,
        itemMetadatas: [SendableItemMetadata],
        handleInvalidParent: Bool = true
    ) {
        Task {
            do {
                let items = try await itemMetadatas.toFileProviderItems(
                    account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log
                )

                Task { @MainActor in
                    observer.didEnumerate(items)
                    logger.info("Did enumerate \(items.count) items. Next page is nil: \(nextPage == nil)")

                    if let nextPage, let nextPageData = try? JSONEncoder().encode(nextPage) {
                        logger.info("Next page: \(String(data: nextPageData, encoding: .utf8) ?? "?")")
                        observer.finishEnumerating(upTo: NSFileProviderPage(nextPageData))
                    } else {
                        observer.finishEnumerating(upTo: nil)
                    }
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.info("Not handling invalid parent in enumeration.")
                    observer.finishEnumeratingWithError(error)
                    return
                }

                do {
                    let metadata = try await attemptInvalidParentRecovery(
                        error: error,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )

                    completeEnumerationObserver(
                        observer,
                        nextPage: nextPage,
                        itemMetadatas: [metadata] + itemMetadatas,
                        handleInvalidParent: false
                    )
                } catch {
                    observer.finishEnumeratingWithError(error)
                }
            }
        }
    }

    func completeChangesObserver(
        _ observer: NSFileProviderChangeObserver,
        anchor: NSFileProviderSyncAnchor,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        changes: ChangeSet,
        handleInvalidParent: Bool = true
    ) {
        logger.info("Completing change observation...")

        for metadata in changes.created {
            logger.debug("Got added metadata to report.", [.item: metadata.ocId, .name: metadata.fileName])
        }

        for metadata in changes.updated {
            logger.debug("Got updated metadata to report.", [.item: metadata.ocId, .name: metadata.fileName])
        }

        for metadata in changes.deleted {
            logger.debug("Got deleted metadata to report.", [.item: metadata.ocId, .name: metadata.fileName])
        }

        // The file provider framework does not differentiate between newly added and updated items, hence the collections are merged.
        // Sort by remote path length (ascending) so parent directories are always reported before
        // their children. Without this ordering, macOS may create the rename-destination folder to
        // house a child item before it processes the parent directory rename, leaving both the old
        // and new folder name visible on disk simultaneously.
        let newAndUpdatedMetadatas: [SendableItemMetadata] = changes.createdAndUpdated
            .sorted { $0.remotePath().count < $1.remotePath().count }

        let deletedMetadatas = changes.deleted
        let deletedFileProviderItemIdentifiers = Array(deletedMetadatas.map {
            NSFileProviderItemIdentifier($0.ocId)
        })

        if deletedFileProviderItemIdentifiers.isEmpty == false {
            observer.didDeleteItems(withIdentifiers: deletedFileProviderItemIdentifiers)
        }

        Task { [newAndUpdatedMetadatas, deletedMetadatas] in
            do {
                let updatedItems = try await newAndUpdatedMetadatas.toFileProviderItems(account: account, remoteInterface: remoteInterface, dbManager: dbManager, log: self.logger.log)

                Task { @MainActor in
                    if !updatedItems.isEmpty {
                        observer.didUpdate(updatedItems)
                    }

                    observer.finishEnumeratingChanges(upTo: anchor, moreComing: false)

                    for metadata in deletedMetadatas {
                        dbManager.removeItemMetadata(ocId: metadata.ocId)
                    }
                }
            } catch let error as NSError { // This error can only mean a missing parent item identifier
                guard handleInvalidParent else {
                    logger.error("Not handling invalid parent in change enumeration!")
                    observer.finishEnumeratingWithError(error)
                    return
                }

                logger.info("Attempting handling invalid parent in change enumeration.")

                do {
                    let metadata = try await attemptInvalidParentRecovery(
                        error: error,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager
                    )

                    var recoveredChanges = changes
                    recoveredChanges.created.append(metadata)

                    completeChangesObserver(
                        observer,
                        anchor: anchor,
                        enumeratedItemIdentifier: enumeratedItemIdentifier,
                        account: account,
                        remoteInterface: remoteInterface,
                        dbManager: dbManager,
                        changes: recoveredChanges,
                        handleInvalidParent: false
                    )
                } catch {
                    observer.finishEnumeratingWithError(error)
                }
            }

            logger.info("Completed change observation.")
        }
    }

    func attemptInvalidParentRecovery(
        error: NSError,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) async throws -> SendableItemMetadata {
        logger.info("Attempting recovery from invalid parent identifier.")
        // Try to recover from errors involving missing metadata for a parent
        let userInfoKey =
            FilesDatabaseManager.ErrorUserInfoKey.missingParentServerUrlAndFileName.rawValue
        guard let urlToEnumerate = (error as NSError).userInfo[userInfoKey] as? String else {
            logger.fault("No missing parent server url and filename in error user info.")
            assertionFailure()
            throw NSError()
        }

        logger.info("Recovering from invalid parent identifier at \(urlToEnumerate)")

        let readResult = await Enumerator.readServerUrl(
            urlToEnumerate,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            depth: .target,
            log: logger.log
        )
        guard readResult.error == nil || readResult.error == .success,
              let metadata = readResult.metadatas?.first
        else {
            logger.error(
                """
                Problem retrieving parent for metadata.
                    Error: \(readResult.error?.errorDescription ?? "NONE")
                    Metadatas: \(readResult.metadatas?.count ?? -1)
                """
            )

            throw readResult.error?.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
        }
        // Provide it to the caller method so it can ingest it into the database and fix future errs
        return metadata
    }
}
