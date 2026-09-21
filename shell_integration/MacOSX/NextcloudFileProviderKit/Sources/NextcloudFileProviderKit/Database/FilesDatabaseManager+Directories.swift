//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB

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

        return read("Could not fetch the items below a directory.", [.url: directoryServerUrl]) { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.hasServerUrl(equalTo: directoryServerUrl, includingDescendants: true))
                .fetchRecords(db, logger: logger)
                .map(\.metadata)
        } ?? []
    }

    ///
    /// Immediate children of a directory — i.e. items whose parent ``serverUrl``
    /// equals the directory's full path. Unlike ``childItems(directoryMetadata:)``
    /// this does not recurse into descendants.
    ///
    func immediateChildItems(directoryMetadata: SendableItemMetadata) -> [SendableItemMetadata] {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)

        return read("Could not fetch the immediate children of a directory.", [.url: directoryServerUrl]) { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.hasServerUrl(equalTo: directoryServerUrl, includingDescendants: false))
                .fetchRecords(db, logger: logger)
                .map(\.metadata)
        } ?? []
    }

    ///
    /// The number of **direct** children of a directory, as vended by `Item.childItemCount`,
    /// excluding tombstones, other accounts' rows and the container's own row, which for the root
    /// carries the same `serverUrl` its children do.
    ///
    func childItemCount(directoryMetadata: SendableItemMetadata) -> Int {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)
        let account = directoryMetadata.account
        let ocId = directoryMetadata.ocId

        return read("Could not count the items below a directory.", [.url: directoryServerUrl]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.account == account
                        && ItemMetadataRecord.Columns.deleted == false
                        && ItemMetadataRecord.Columns.ocId != ocId
                        && ItemMetadataRecord.hasServerUrl(equalTo: directoryServerUrl, includingDescendants: false)
                )
                .fetchCount(db)
        } ?? 0
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
    func hasEvictableDescendantFile(directoryMetadata: SendableItemMetadata) -> Bool {
        let directoryServerUrl = fullServerPathUrl(for: directoryMetadata)

        return read("Could not look up evictable descendants of a directory.", [.url: directoryServerUrl]) { db in
            try !ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.isEvictableFile
                        && ItemMetadataRecord.hasServerUrl(equalTo: directoryServerUrl, includingDescendants: true)
                )
                .isEmpty(db)
        } ?? false
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

        return read("Could not fetch evictable descendants of a directory.", [.url: directoryServerUrl]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.isEvictableFile
                        && ItemMetadataRecord.hasServerUrl(equalTo: directoryServerUrl, includingDescendants: true)
                )
                .select(ItemMetadataRecord.Columns.ocId, as: String.self)
                .fetchAll(db)
                .map { NSFileProviderItemIdentifier($0) }
        } ?? []
    }

    ///
    /// Identifiers of the files below a directory whose status is below `status`, for example everything which is not part of an upload in progress.
    ///
    func descendantFileOcIds(underRemotePath remotePath: String, account: String, statusBelow status: Status) -> [String] {
        read("Could not fetch the files below a directory.", [.url: remotePath]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.directory == false
                        && ItemMetadataRecord.Columns.account == account
                        && ItemMetadataRecord.Columns.status < status.rawValue
                        && ItemMetadataRecord.hasServerUrl(equalTo: remotePath, includingDescendants: true)
                )
                .select(ItemMetadataRecord.Columns.ocId, as: String.self)
                .fetchAll(db)
        } ?? []
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
        read("Could not look up directory metadata.", [.item: ocId]) { db in
            try directoryMetadata(ocId: ocId, in: db)?.metadata
        } ?? nil
    }

    /// Deletes all metadatas related to the info of the directory provided
    func deleteDirectoryAndSubdirectoriesMetadata(
        ocId: String
    ) -> [SendableItemMetadata]? {
        write("Failure to delete directory metadata in recursive delete.", [.item: ocId]) { db in
            guard var directoryMetadata = try directoryMetadata(ocId: ocId, in: db) else {
                logger.error("Could not find directory metadata for ocId. Not proceeding with deletion.", [.item: ocId])
                return nil
            }

            let directoryMetadataCopy = directoryMetadata.metadata
            let directoryOcId = directoryMetadata.ocId
            let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
            let directoryAccount = directoryMetadata.account
            let directoryEtag = directoryMetadata.etag

            logger.debug("Deleting root directory metadata in recursive delete.", [.eTag: directoryEtag, .item: directoryMetadata.ocId, .url: directoryUrlPath])

            directoryMetadata.deleted = true
            try directoryMetadata.upsertRow(db)

            var deletedMetadatas: [SendableItemMetadata] = [directoryMetadataCopy]

            let results = try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.account == directoryAccount
                        && ItemMetadataRecord.hasServerUrl(equalTo: directoryUrlPath, includingDescendants: true)
                )
                .fetchRecords(db, logger: logger)

            // TODO: Parent is deleted even when a child upload is pending. The child will
            // orphan after upload. Follow-up: defer parent deletion or re-parent after upload.
            for var result in results {
                if result.status >= Status.inUpload.rawValue {
                    // Preserve the child metadata and its resumable chunks. Item deletion applies
                    // the same status boundary when deciding which descendant chunks to clean up.
                    logger.info("Skipping deletion of child with pending upload.", [.item: result.ocId])
                    continue
                }
                if result.isLockFileOfLocalOrigin {
                    logger.info("Skipping deletion of local-origin lock file during directory delete.", [.item: result.ocId, .name: result.fileName])
                    continue
                }
                let inactiveItemMetadata = result.metadata
                result.deleted = true
                try result.upsertRow(db)
                deletedMetadatas.append(inactiveItemMetadata)
            }

            logger.debug("Completed deletions in directory recursive delete.", [.eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])

            return deletedMetadatas
        } ?? nil
    }

    func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String
    ) -> [SendableItemMetadata]? {
        write("Could not rename directory metadata.", [.item: ocId, .url: newServerUrl]) { db in
            try renameDirectoryAndPropagateToChildren(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName, in: db)
        } ?? nil
    }
}

extension FilesDatabaseManager {
    func directoryMetadata(ocId: String, in db: Database) throws -> ItemMetadataRecord? {
        try ItemMetadataRecord
            .filter(key: ocId)
            .filter(ItemMetadataRecord.Columns.directory == true)
            .fetchRecord(db, logger: logger)
    }

    func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String, in db: Database
    ) throws -> [SendableItemMetadata]? {
        guard let directoryMetadata = try directoryMetadata(ocId: ocId, in: db) else {
            logger.error("Could not find a directory with ocID \(ocId), cannot proceed with recursive renaming.", [.item: ocId])
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldItemFilename = directoryMetadata.fileName
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + oldItemFilename
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let childItemResults = try ItemMetadataRecord
            .filter(
                ItemMetadataRecord.Columns.account == directoryMetadata.account
                    && ItemMetadataRecord.hasServerUrl(equalTo: oldDirectoryServerUrl, includingDescendants: true)
            )
            .fetchRecords(db, logger: logger)

        try renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName, in: db)
        logger.debug("Renamed root renaming directory from \"\(oldDirectoryServerUrl)\" to \"\(newDirectoryServerUrl)\".", [.item: ocId])

        for var childItem in childItemResults {
            let oldServerUrl = childItem.serverUrl
            let movedServerUrl = oldServerUrl.replacingOccurrences(
                of: oldDirectoryServerUrl, with: newDirectoryServerUrl
            )
            childItem.updateLocation(serverUrl: movedServerUrl, fileName: childItem.fileName)
            childItem.lockToken = nil
            try childItem.upsertRow(db)
            logger.debug(
                """
                Moved childItem at: \(oldServerUrl)
                                to: \(movedServerUrl)
                """
            )
        }

        return try ItemMetadataRecord
            .filter(
                ItemMetadataRecord.Columns.account == directoryMetadata.account
                    && ItemMetadataRecord.hasServerUrl(equalTo: newDirectoryServerUrl, includingDescendants: true)
            )
            .fetchRecords(db, logger: logger)
            .map(\.metadata)
    }
}
