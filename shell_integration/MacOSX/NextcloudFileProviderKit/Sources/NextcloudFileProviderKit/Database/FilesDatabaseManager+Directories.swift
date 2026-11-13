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

    /// Mark the directory and everything below it deleted.
    ///
    /// - Parameters:
    ///   - ocId: The directory.
    ///   - stampingSyncTime: When given, every row marked here gets this `syncTime`, so the working-set derivation finds the deletions by date. The default leaves the stamps alone.
    ///
    /// - Returns: The rows as they were before, or `nil` when the directory is unknown.
    ///
    func deleteDirectoryAndSubdirectoriesMetadata(
        ocId: String, stampingSyncTime: Date? = nil
    ) -> [SendableItemMetadata]? {
        write("Failure to delete directory metadata in recursive delete.", [.item: ocId]) { db in
            try deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId, stampingSyncTime: stampingSyncTime, in: db)
        } ?? nil
    }
    private func deleteDirectoryAndSubdirectoriesMetadata(
        ocId: String, stampingSyncTime: Date?, in db: Database
    ) throws -> [SendableItemMetadata]? {
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
        if let stampingSyncTime {
            directoryMetadata.syncTime = stampingSyncTime
        }
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
            if result.isAtHoldingPath || result.serverUrl.contains(FilesDatabaseManager.holdingNameSeparator) {
                // A folder parked at a holding path, and what waits below it, belong to the placement to come.
                logger.info("Skipping deletion of an item waiting at a holding path.", [.item: result.ocId, .name: result.fileName])
                continue
            }
            let inactiveItemMetadata = result.metadata
            result.deleted = true
            if let stampingSyncTime {
                result.syncTime = stampingSyncTime
            }
            try result.upsertRow(db)
            deletedMetadatas.append(inactiveItemMetadata)
        }

        logger.debug("Completed deletions in directory recursive delete.", [.eTag: directoryEtag, .item: directoryOcId, .url: directoryUrlPath])

        return deletedMetadatas
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
    /// The character between a parked directory's name and its identifier in its holding name, doubled when the directory was already deleted.
    static let holdingNameSeparator = "\u{1}"

    func directoryMetadata(ocId: String, in db: Database) throws -> ItemMetadataRecord? {
        try ItemMetadataRecord
            .filter(key: ocId)
            .filter(ItemMetadataRecord.Columns.directory == true)
            .fetchRecord(db, logger: logger)
    }

    ///
    /// Put `incoming`, a directory about to be written, at its location without mixing up the contents of folders.
    ///
    /// Descendants are found by their path, so a location holds at most one directory row. Any other directory stored at the
    /// location, live or deleted, is parked first, and a stored row of `incoming` at another location then moves there with
    /// its own descendants. A deleted stored row leaves its deleted descendants, and everything below them, behind, so each is
    /// written again where the server lists it; a row parked while live counts as live. A deleted stored row that shares its
    /// location with a live directory moves alone, because its descendants cannot be told apart from that directory's, so its
    /// own live descendants, such as pending uploads and local lock files, stay with that directory.
    ///
    /// - Returns: The descendants of `incoming` that moved with it.
    ///
    func placeDirectory(_ incoming: any ItemMetadata, in db: Database) throws -> [SendableItemMetadata] {
        guard incoming.directory, incoming.ocId != NSFileProviderItemIdentifier.rootContainer.rawValue else {
            return []
        }

        try parkDirectories(at: incoming.serverUrl, fileName: incoming.fileName, otherThan: incoming.ocId, in: db)

        guard let stored = try directoryMetadata(ocId: incoming.ocId, in: db), !stored.hasSameLocation(as: incoming) else {
            return []
        }

        let parkedWhileLive = stored.fileName.hasSuffix(Self.holdingNameSeparator + stored.ocId) && !stored.fileName.hasSuffix(Self.holdingNameSeparator + Self.holdingNameSeparator + stored.ocId)
        let countsAsLive = !stored.deleted || parkedWhileLive

        let movesAlone = try !countsAsLive && ItemMetadataRecord
            .filter(
                ItemMetadataRecord.hasLocation(serverUrl: stored.serverUrl, fileName: stored.fileName)
                    && ItemMetadataRecord.Columns.directory == true
                    && ItemMetadataRecord.Columns.deleted == false
                    && ItemMetadataRecord.Columns.ocId != stored.ocId
            )
            .fetchCount(db) > 0

        guard !movesAlone else {
            try renameItemMetadata(ocId: incoming.ocId, newServerUrl: incoming.serverUrl, newFileName: incoming.fileName, in: db)
            return []
        }

        return try renameDirectoryAndPropagateToChildren(ocId: incoming.ocId, newServerUrl: incoming.serverUrl, newFileName: incoming.fileName, includingDeleted: countsAsLive, in: db) ?? []
    }

    ///
    /// Move every other directory stored at a location, with its descendants, to a name of its own and mark it deleted.
    ///
    /// The parked row keeps its descendants together, so if the server lists that folder again somewhere else, it moves there
    /// with only its own contents. Nothing is parked when the directory `ocId` is already stored at the location, because
    /// the descendants of the two directories cannot then be told apart.
    ///
    func parkDirectories(at serverUrl: String, fileName: String, otherThan ocId: String, in db: Database) throws {
        guard try ItemMetadataRecord.filter(key: ocId).filter(ItemMetadataRecord.hasLocation(serverUrl: serverUrl, fileName: fileName)).fetchCount(db) == 0 else {
            return
        }

        let occupants = try ItemMetadataRecord
            .filter(
                ItemMetadataRecord.hasLocation(serverUrl: serverUrl, fileName: fileName)
                    && ItemMetadataRecord.Columns.directory == true
                    && ItemMetadataRecord.Columns.ocId != ocId
            )
            .fetchRecords(db, logger: logger)

        for occupant in occupants {
            let separator = occupant.deleted ? Self.holdingNameSeparator + Self.holdingNameSeparator : Self.holdingNameSeparator
            _ = try renameDirectoryAndPropagateToChildren(ocId: occupant.ocId, newServerUrl: occupant.serverUrl, newFileName: occupant.fileName + separator + occupant.ocId, in: db)

            try ItemMetadataRecord
                .filter(key: occupant.ocId)
                .updateAll(db, ItemMetadataRecord.Columns.deleted.set(to: true), ItemMetadataRecord.Columns.syncTime.set(to: Date().timeIntervalSinceReferenceDate))

            logger.info("Parked a directory whose location another directory takes.", [.item: occupant.ocId, .name: occupant.fileName, .url: occupant.serverUrl])
        }
    }

    func renameDirectoryAndPropagateToChildren(
        ocId: String, newServerUrl: String, newFileName: String, includingDeleted: Bool = true, in db: Database
    ) throws -> [SendableItemMetadata]? {
        guard let directoryMetadata = try directoryMetadata(ocId: ocId, in: db) else {
            logger.error("Could not find a directory with ocID \(ocId), cannot proceed with recursive renaming.", [.item: ocId])
            return nil
        }

        let oldItemServerUrl = directoryMetadata.serverUrl
        let oldItemFilename = directoryMetadata.fileName
        let oldDirectoryServerUrl = oldItemServerUrl + "/" + oldItemFilename
        let newDirectoryServerUrl = newServerUrl + "/" + newFileName
        let descendants = try ItemMetadataRecord
            .filter(
                ItemMetadataRecord.Columns.account == directoryMetadata.account
                    && ItemMetadataRecord.hasServerUrl(equalTo: oldDirectoryServerUrl, includingDescendants: true)
            )
            .fetchRecords(db, logger: logger)

        // A deleted folder that stays behind keeps everything below it, so every moved row still has its parent.
        let pathsLeftBehind = includingDeleted ? [] : descendants.filter { $0.deleted && $0.directory }.map { $0.serverUrl + "/" + $0.fileName }
        let childItemResults = includingDeleted ? descendants : descendants.filter { child in
            !child.deleted && !pathsLeftBehind.contains { child.serverUrl == $0 || child.serverUrl.hasPrefix($0 + "/") }
        }

        try renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName, in: db)
        logger.debug("Renamed root renaming directory from \"\(oldDirectoryServerUrl)\" to \"\(newDirectoryServerUrl)\".", [.item: ocId])

        var movedChildren: [SendableItemMetadata] = []
        movedChildren.reserveCapacity(childItemResults.count)

        for var childItem in childItemResults {
            let oldServerUrl = childItem.serverUrl
            let movedServerUrl = oldServerUrl.replacingOccurrences(
                of: oldDirectoryServerUrl, with: newDirectoryServerUrl
            )
            childItem.updateLocation(serverUrl: movedServerUrl, fileName: childItem.fileName)
            childItem.lockToken = nil
            childItem.syncTime = Date()
            try childItem.upsertRow(db)
            movedChildren.append(childItem.metadata)
            logger.debug(
                """
                Moved childItem at: \(oldServerUrl)
                                to: \(movedServerUrl)
                """
            )
        }

        return movedChildren
    }
}
