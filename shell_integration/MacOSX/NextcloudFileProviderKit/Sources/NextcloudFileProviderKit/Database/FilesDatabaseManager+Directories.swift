//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

public extension FilesDatabaseManager {
    private func fullServerPathUrl(for metadata: any ItemMetadata) -> String {
        if metadata.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue {
            metadata.serverUrl
        } else {
            metadata.serverUrl + "/" + metadata.fileName
        }
    }

    func childItems(directoryMetadata: SendableItemMetadata) -> [SendableItemMetadata] {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where {
                $0.serverUrl == directoryServerUrl ||
                    $0.serverUrl.starts(with: directoryServerUrl + "/")
            }
            .toUnmanagedResults()
    }

    ///
    /// Immediate children of a directory — i.e. items whose parent ``serverUrl``
    /// equals the directory's full path. Unlike ``childItems(directoryMetadata:)``
    /// this does not recurse into descendants.
    ///
    func immediateChildItems(directoryMetadata: SendableItemMetadata) -> [SendableItemMetadata] {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)

        return itemMetadatas
            .where { $0.serverUrl == directoryServerUrl }
            .toUnmanagedResults()
    }

    func childItemCount(directoryMetadata: SendableItemMetadata) -> Int {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where {
                $0.serverUrl == directoryServerUrl ||
                    $0.serverUrl.starts(with: directoryServerUrl + "/")
            }
            .count
    }

    ///
    /// Whether `directoryMetadata` holds at least one **evictable descendant file**:
    /// a non-directory row under the directory's path that is downloaded, not
    /// soft-deleted, and NOT individually pinned via "Always keep downloaded".
    ///
    /// Gates the "Remove downloaded items" folder action and folds into the
    /// directory's `itemVersion` (#10085). The clauses:
    /// - `directory == false`: only files carry real materialized payload; a
    ///   sub-directory's `visitedDirectory` means "enumerated", not "downloaded".
    /// - `deleted == false`: a stale tombstone must not count as removable content.
    /// - `keepDownloaded == false`: a descendant the user explicitly pinned is not
    ///   removable — offering to remove it would be wrong, and evicting a
    ///   strict-pinned item is refused by the framework with -2008 NonEvictable
    ///   (#9891). Pinning propagates the flag to every descendant, so this also
    ///   excludes items strict-pinned via an ancestor.
    /// The `+ "/"` on the prefix branch keeps a sibling like `.../folderX` from
    /// matching the directory `.../folder`.
    ///
    /// Uses `.first` for a bounded, early-out existence check — Realm evaluates
    /// the query lazily and does not build the full descendant result set.
    ///
    func hasEvictableDescendantFile(directoryMetadata: SendableItemMetadata) -> Bool {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where {
                $0.directory == false &&
                    $0.downloaded == true &&
                    $0.deleted == false &&
                    $0.keepDownloaded == false &&
                    ($0.serverUrl == directoryServerUrl ||
                        $0.serverUrl.starts(with: directoryServerUrl + "/"))
            }
            .first != nil
    }

    ///
    /// Identifiers of every evictable descendant file of `directoryMetadata`
    /// (same predicate as ``hasEvictableDescendantFile(directoryMetadata:)``).
    ///
    /// Used by the folder "Remove downloaded items" handler to evict descendants
    /// individually rather than calling `evictItem` on the directory: this honors
    /// any individually-pinned descendant (left materialized) and never asks the
    /// framework to evict strict-pinned content, avoiding -2008 NonEvictable
    /// (#9891, #10085).
    ///
    func evictableDescendantFileIdentifiers(directoryMetadata: SendableItemMetadata) -> [NSFileProviderItemIdentifier] {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        return itemMetadatas
            .where {
                $0.directory == false &&
                    $0.downloaded == true &&
                    $0.deleted == false &&
                    $0.keepDownloaded == false &&
                    ($0.serverUrl == directoryServerUrl ||
                        $0.serverUrl.starts(with: directoryServerUrl + "/"))
            }
            .map { NSFileProviderItemIdentifier($0.ocId) }
    }

    ///
    /// The identifiers of every ancestor container of each given **file**, up to
    /// and including the root container.
    ///
    /// A folder's `displayEvict` ("Remove download") visibility depends on whether
    /// it holds a materialized descendant file, so when a file materializes or is
    /// evicted the framework's cached snapshot of each container on the path to
    /// the root must be refreshed (#10085). The root container is included so its
    /// "Remove download" can evict everything materialized in the domain —
    /// symmetric with pinning the whole file provider "Always keep downloaded".
    /// This returns the set to nudge; the framework signaling is the extension's
    /// job. Non-file ocIds are ignored — only a file's materialization state feeds
    /// an ancestor's `displayEvict`.
    ///
    /// The trash container is excluded: trashed items are not subject to
    /// on-demand eviction. `parentItemIdentifierFromMetadata` maps the account
    /// root to `.rootContainer`, which terminates the walk cleanly.
    ///
    func ancestorContainerIdentifiers(ofFileItemsWithOcIds ocIds: Set<String>) -> Set<NSFileProviderItemIdentifier> {
        var ancestors = Set<NSFileProviderItemIdentifier>()

        for ocId in ocIds {
            guard let start = itemMetadata(ocId: ocId), start.directory == false else { continue }

            var current: SendableItemMetadata? = start
            while let metadata = current, let parent = parentItemIdentifierFromMetadata(metadata) {
                // Trashed items are not subject to on-demand eviction.
                guard parent != .trashContainer else { break }
                // Memoize: if this ancestor is already collected, its entire chain
                // to the root was walked by an earlier file — stop re-walking it.
                // Collapses O(files × depth) into ~O(distinct nodes) for a large
                // materialized set full of siblings sharing directories.
                let (inserted, _) = ancestors.insert(parent)
                guard inserted else { break }
                // The root container has no further walkable parent — stop once added.
                guard parent != .rootContainer else { break }
                current = directoryMetadata(ocId: parent.rawValue)
            }
        }

        return ancestors
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: SendableItemMetadata) -> SendableItemMetadata? {
        self.itemMetadata(account: itemMetadata.account, locatedAtRemoteUrl: itemMetadata.serverUrl)
    }

    func directoryMetadata(ocId: String) -> SendableItemMetadata? {
        if let metadata = itemMetadatas.where({ $0.ocId == ocId && $0.directory }).first {
            return SendableItemMetadata(value: metadata)
        }

        return nil
    }

    /// Deletes all metadatas related to the info of the directory provided
    func deleteDirectoryAndSubdirectoriesMetadata(
        ocId: String
    ) -> [SendableItemMetadata]? {
        guard let directoryMetadata = itemMetadatas
            .where({ $0.ocId == ocId && $0.directory })
            .first
        else {
            logger.error("Could not find directory metadata for ocId. Not proceeding with deletion.", [.item: ocId])
            return nil
        }

        let directoryMetadataCopy = SendableItemMetadata(value: directoryMetadata)
        let directoryOcId = directoryMetadata.ocId
        let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let directoryAccount = directoryMetadata.account
        let directoryEtag = directoryMetadata.etag

        logger.debug("Deleting root directory metadata in recursive delete.", [.eTag: directoryEtag, .item: directoryMetadata.ocId, .url: directoryUrlPath])

        let database = ncDatabase()
        do {
            try database.write { directoryMetadata.deleted = true }
        } catch {
            logger.error("Failure to delete root directory metadata in recursive delete.", [.error: error, .eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])
            return nil
        }

        var deletedMetadatas: [SendableItemMetadata] = [directoryMetadataCopy]

        let results = itemMetadatas.where {
            $0.account == directoryAccount &&
                ($0.serverUrl == directoryUrlPath || $0.serverUrl.starts(with: directoryUrlPath + "/"))
        }

        // TODO: Parent is deleted even when a child upload is pending. The child will
        // orphan after upload. Follow-up: defer parent deletion or re-parent after upload.
        for result in results {
            if result.status >= Status.inUpload.rawValue {
                logger.info("Skipping deletion of child with pending upload.", [.item: result.ocId])
                continue
            }
            if result.isLockFileOfLocalOrigin {
                logger.info("Skipping deletion of local-origin lock file during directory delete.", [.item: result.ocId, .name: result.fileName])
                continue
            }
            let inactiveItemMetadata = SendableItemMetadata(value: result)
            do {
                try database.write { result.deleted = true }
                deletedMetadatas.append(inactiveItemMetadata)
            } catch {
                logger.error("Failure to delete directory metadata child in recursive delete", [.error: error, .eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])
            }
        }

        logger.debug("Completed deletions in directory recursive delete.", [.eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])

        return deletedMetadatas
    }

    func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String
    ) -> [SendableItemMetadata]? {
        guard let directoryMetadata = itemMetadatas
            .where({ $0.ocId == ocId && $0.directory })
            .first
        else {
            logger.error("Could not find a directory with ocID \(ocId), cannot proceed with recursive renaming.", [.item: ocId])
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldItemFilename = directoryMetadata.fileName
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + oldItemFilename
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = itemMetadatas.where {
            $0.account == directoryMetadata.account &&
                ($0.serverUrl == oldDirectoryServerUrl || $0.serverUrl.starts(with: oldDirectoryServerUrl + "/"))
        }

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        logger.debug("Renamed root renaming directory from \"\(oldDirectoryServerUrl)\" to \"\(newDirectoryServerUrl)\".", [.item: ocId])

        do {
            let database = ncDatabase()
            try database.write {
                for childItem in childItemResults {
                    let oldServerUrl = childItem.serverUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(
                        of: oldDirectoryServerUrl, with: newDirectoryServerUrl
                    )
                    childItem.serverUrl = movedServerUrl
                    childItem.normalizedServerUrl = movedServerUrl.canonicalForm
                    childItem.lockToken = nil
                    database.add(childItem, update: .all)
                    logger.debug(
                        """
                        Moved childItem at: \(oldServerUrl)
                                        to: \(movedServerUrl)
                        """
                    )
                }
            }
        } catch {
            logger.error("Could not rename directory metadata.", [.error: error, .item: ocId, .url: newServerUrl])
            return nil
        }

        return itemMetadatas
            .where {
                $0.account == directoryMetadata.account &&
                    ($0.serverUrl == newDirectoryServerUrl || $0.serverUrl.starts(with: newDirectoryServerUrl + "/"))
            }
            .toUnmanagedResults()
    }
}
