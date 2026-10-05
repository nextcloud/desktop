//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB

///
/// The file provider domain's metadata database.
///
/// Every method enters the database exactly once, through ``read(_:_:)`` or ``write(_:_:)``, and does its work in a worker which takes the open `Database`. Workers call only other workers, because the database access methods are not reentrant. Results leave the database as value types.
///
public final class FilesDatabaseManager: Sendable {
    public enum ErrorCode: Int {
        case metadataNotFound = -1000
        case parentMetadataNotFound = -1001
    }

    public enum ErrorUserInfoKey: String {
        case missingParentServerUrlAndFileName = "MissingParentServerUrlAndFileName"
    }

    static let errorDomain = "FilesDatabaseManager"

    static func error(code: ErrorCode, userInfo: [String: String]) -> NSError {
        NSError(domain: errorDomain, code: code.rawValue, userInfo: userInfo)
    }

    static func parentMetadataNotFoundError(itemUrl: String) -> NSError {
        error(
            code: .parentMetadataNotFound,
            userInfo: [ErrorUserInfoKey.missingParentServerUrlAndFileName.rawValue: itemUrl]
        )
    }

    /// Longest list bound into one `IN (...)` clause, well below SQLite's variable limit.
    static let inClauseChunkSize = 500

    let logger: FileProviderLogger
    let account: Account

    /// The open database: a pool on the domain's file, or an in-memory queue when no directory is available.
    let writer: any DatabaseWriter

    /// Location of the database file, `nil` when the database lives in memory.
    let databaseURL: URL?

    ///
    /// Open the domain's database, creating, importing or migrating it as needed.
    ///
    /// A database left behind by a newer build is set up from scratch (see ``StoreVersionGuard``); a Realm database left behind by an older build is imported (see ``RealmStoreImporter``).
    ///
    /// - Parameters:
    ///     - account: The Nextcloud account for which the database is being created.
    ///     - customDatabaseDirectory: Optional custom directory where the database files should be stored. If not provided, the default directory will be used.
    ///     - fileProviderDomainIdentifier: The domain whose data this database holds; also names the file.
    ///     - log: The log to write to.
    ///     - defaults: Where the domain's settings, including the last seen store version, are kept.
    ///
    public init(account: Account, databaseDirectory customDatabaseDirectory: URL? = nil, fileProviderDomainIdentifier: NSFileProviderDomainIdentifier, log: any FileProviderLogging, defaults: UserDefaults = .standard) {
        self.account = account
        logger = FileProviderLogger(category: "FilesDatabaseManager", log: log)

        let defaultDatabaseDirectory = FileManager.default.fileProviderDomainSupportDirectory(for: fileProviderDomainIdentifier)

        guard let databaseDirectory = customDatabaseDirectory ?? defaultDatabaseDirectory else {
            logger.fault("Neither custom nor default database directory defined! Metadata will not be persisted.")
            databaseURL = nil
            writer = Self.openInMemoryDatabase(logger: logger)
            return
        }

        let databaseLocation = databaseDirectory
            .appendingPathComponent(fileProviderDomainIdentifier.rawValue)
            .appendingPathExtension(DatabaseSchema.fileExtension)
        databaseURL = databaseLocation

        var domainDefaults = FileProviderDomainDefaults(identifier: fileProviderDomainIdentifier, log: log, defaults: defaults)
        let seenVersion = max(domainDefaults.latestSeenDatabaseVersion ?? 0, StoreVersionGuard.recordedVersion(at: databaseLocation) ?? 0)

        switch StoreVersionGuard.decide(seen: seenVersion) {
            case let .downgrade(from):
                logger.fault("The metadata database was written by a newer build. Setting it up from scratch.", [.url: databaseLocation.path, .name: "store version \(from), supported \(StoreVersion.current)"])
                StoreVersionGuard.resetStore(at: databaseLocation, logger: logger)
            case let .upgrade(from):
                logger.info("Migrating the metadata database.", [.url: databaseLocation.path, .name: "store version \(from) to \(StoreVersion.current)"])
            case .fresh, .current:
                break
        }

        let realmLocation = databaseDirectory
            .appendingPathComponent(fileProviderDomainIdentifier.rawValue)
            .appendingPathExtension("realm")

        if RealmStoreImporter.importIfNeeded(realmURL: realmLocation, sqliteURL: databaseLocation, logger: logger) == .failed {
            // Nothing is persisted until the import succeeds on a later start, so no state can accumulate which that import would then replace.
            logger.fault("The Realm database could not be imported. Metadata is kept in memory until the next start.", [.url: realmLocation.path])
            writer = Self.openInMemoryDatabase(logger: logger)
            return
        }

        writer = Self.openDatabase(at: databaseLocation, label: fileProviderDomainIdentifier.rawValue, logger: logger)
        logger.info("Opened metadata database.", [.url: databaseLocation.path])

        if writer is DatabasePool {
            domainDefaults.latestSeenDatabaseVersion = StoreVersion.current
        }

        repairPersistedLogicalAddresses()
    }

    // MARK: - Opening

    static func configuration(label: String) -> Configuration {
        var configuration = Configuration()
        configuration.label = "FilesDatabaseManager.\(label)"
        configuration.busyMode = .timeout(5)
        configuration.maximumReaderCount = 8
        configuration.prepareDatabase { db in
            // Durable against crashes in WAL mode; only a power loss can lose the last commits, and this store is rebuilt from the server plus local flags.
            try db.execute(sql: "PRAGMA synchronous = NORMAL")
        }
        return configuration
    }

    /// Open the file, migrating its schema.
    ///
    /// A file SQLite reports as corrupt is set aside and replaced. Any other failure, such as a busy or full disk, keeps the file untouched and serves this process from memory, so the next start can try again.
    ///
    private static func openDatabase(at url: URL, label: String, logger: FileProviderLogger) -> any DatabaseWriter {
        do {
            return try openPool(at: url, label: label)
        } catch {
            guard isCorruptionError(error) else {
                logger.fault("Could not open the metadata database. Metadata is kept in memory until the next start.", [.url: url.path, .error: error])
                return openInMemoryDatabase(logger: logger)
            }

            logger.fault("The metadata database is corrupt. Setting the file aside and starting with an empty one.", [.url: url.path, .error: error])
        }

        setAsideUnreadableDatabase(at: url, logger: logger)

        do {
            return try openPool(at: url, label: label)
        } catch {
            logger.fault("Could not open a fresh metadata database. Metadata is kept in memory until the next start.", [.url: url.path, .error: error])
            return openInMemoryDatabase(logger: logger)
        }
    }

    private static func isCorruptionError(_ error: Error) -> Bool {
        guard let databaseError = error as? DatabaseError else {
            return false
        }

        return [ResultCode.SQLITE_CORRUPT, .SQLITE_NOTADB].contains(databaseError.resultCode.primaryResultCode)
    }

    private static func openPool(at url: URL, label: String) throws -> any DatabaseWriter {
        let pool = try DatabasePool(path: url.path, configuration: configuration(label: label))
        try DatabaseSchema.migrator.migrate(pool)
        try pool.write { db in
            try db.execute(sql: "PRAGMA user_version = \(StoreVersion.current)")
        }
        return pool
    }

    private static func openInMemoryDatabase(logger _: FileProviderLogger) -> any DatabaseWriter {
        do {
            let queue = try DatabaseQueue(configuration: configuration(label: "memory"))
            try DatabaseSchema.migrator.migrate(queue)
            return queue
        } catch {
            fatalError("Could not open an in-memory metadata database: \(error)")
        }
    }

    /// Rename the database file and its journal files so the next open starts from scratch and the broken file stays available for support.
    private static func setAsideUnreadableDatabase(at url: URL, logger: FileProviderLogger) {
        let suffix = ".unreadable-" + ISO8601DateFormatter().string(from: Date())

        for path in [url.path, url.path + "-wal", url.path + "-shm"] where FileManager.default.fileExists(atPath: path) {
            do {
                try FileManager.default.moveItem(atPath: path, toPath: path + suffix)
            } catch {
                logger.error("Could not set aside an unreadable database file.", [.url: path, .error: error])
            }
        }
    }

    // MARK: - Access

    /// Run a read; a failure is logged with `failure` and yields `nil`.
    func read<T>(_ failure: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], _ body: (Database) throws -> T) -> T? {
        do {
            return try writer.read(body)
        } catch {
            var details = details
            details[.error] = error
            logger.error(failure, details)
            return nil
        }
    }

    /// Run a write in one transaction; a failure is logged with `failure` and yields `nil`.
    func write<T>(_ failure: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], _ body: (Database) throws -> T) -> T? {
        do {
            return try writer.write(body)
        } catch {
            var details = details
            details[.error] = error
            logger.error(failure, details)
            return nil
        }
    }

    /// Checkpoint the write-ahead log so the file is complete on disk before the process goes away.
    public func checkpointForShutdown() {
        do {
            try writer.writeWithoutTransaction { db in
                try db.checkpoint(.passive)
            }
        } catch {
            logger.error("Could not checkpoint the metadata database.", [.error: error])
        }
    }

    // MARK: - Lookups

    public func anyItemMetadatasForAccount(_ account: String) -> Bool {
        read("Could not look up whether the account has any item metadata.", [.account: account]) { db in
            try !ItemMetadataRecord.filter(ItemMetadataRecord.Columns.account == account).isEmpty(db)
        } ?? false
    }

    public func itemMetadata(ocId: String) -> SendableItemMetadata? {
        read("Could not look up item metadata.", [.item: ocId]) { db in
            try self.itemMetadata(ocId: ocId, in: db)?.metadata
        } ?? nil
    }

    public func itemMetadata(_ identifier: NSFileProviderItemIdentifier) -> SendableItemMetadata? {
        itemMetadata(ocId: identifier.rawValue)
    }

    /// Return whether metadata exists for any numeric WebDAV file ID received from notify-push.
    public func containsAnyItemMetadata(fileIds: Set<String>) -> Bool {
        guard !fileIds.isEmpty else {
            return false
        }

        return read("Could not look up item metadata by file identifiers.") { db in
            for chunk in Array(fileIds).chunked(into: Self.inClauseChunkSize) {
                if try !ItemMetadataRecord.filter(chunk.contains(ItemMetadataRecord.Columns.fileId)).isEmpty(db) {
                    return true
                }
            }

            return false
        } ?? false
    }

    ///
    /// Look up the item metadata by its account identifier and remote address.
    ///
    /// - Parameters:
    ///     - account: The account this item is scoped by.
    ///     - remoteURL: The full remote URL of the item as a `String`.
    ///
    /// - Returns: Metadata related to the item found by the parameters.
    ///
    public func itemMetadata(account: String, locatedAtRemoteUrl rawRemoteURL: String) -> SendableItemMetadata? {
        read("Could not look up item metadata by remote URL.", [.account: account, .url: rawRemoteURL]) { db in
            try self.itemMetadata(account: account, locatedAtRemoteUrl: rawRemoteURL, in: db)?.metadata
        } ?? nil
    }

    ///
    /// Fetch the metadata object for the root container of the given account.
    ///
    /// This is useful for when you have only the `NSFileProviderItemIdentifier.rootContainer` but no `ocId` to look up metadata by.
    ///
    public func rootItemMetadata(account: Account) -> SendableItemMetadata? {
        read("Could not look up the root item metadata.", [.account: account.ncKitAccount]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.account == account.ncKitAccount
                        && ItemMetadataRecord.Columns.directory == true
                        && ItemMetadataRecord.Columns.path == Account.webDavFilesUrlSuffix
                )
                .order(ItemMetadataRecord.Columns.ocId)
                .fetchOne(db)?
                .metadata
        } ?? nil
    }

    public func itemMetadatas(account: String) -> [SendableItemMetadata] {
        read("Could not fetch the account's item metadata.", [.account: account]) { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.Columns.account == account)
                .fetchAll(db)
                .map(\.metadata)
        } ?? []
    }

    public func itemMetadatas(
        account: String, underServerUrl serverUrl: String
    ) -> [SendableItemMetadata] {
        read("Could not fetch the item metadata under a server URL.", [.account: account, .url: serverUrl]) { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.account == account
                        && ItemMetadataRecord.hasServerUrl(equalTo: serverUrl, includingDescendants: true)
                )
                .fetchAll(db)
                .map(\.metadata)
        } ?? []
    }

    // MARK: - Lookup workers

    func itemMetadata(ocId: String, in db: Database) throws -> ItemMetadataRecord? {
        try ItemMetadataRecord.fetchOne(db, key: ocId)
    }

    /// The row at a logical address derived from a full remote URL. A live row wins over a deleted one at the same address.
    func itemMetadata(account: String, locatedAtRemoteUrl rawRemoteURL: String, in db: Database) throws -> ItemMetadataRecord? {
        guard var urlComponents = URLComponents(string: rawRemoteURL) else {
            logger.error("Failed to create URL components from raw remote URL.", [.account: account, .url: rawRemoteURL])
            return nil
        }

        // Clear everything which is not part of the path to be able to derive a prefix which is then removed from the original raw remote URL.
        urlComponents.fragment = nil
        urlComponents.query = nil
        urlComponents.path = ""

        guard let baseURL = urlComponents.url else {
            logger.error("Failed to derive base URL from components.", [.account: account, .url: rawRemoteURL])
            return nil
        }

        guard let basePrefix = baseURL.absoluteString.removingPercentEncoding else {
            logger.error("Failed to derive absolute string from base URL.", [.account: account, .url: rawRemoteURL])
            return nil
        }

        let index = rawRemoteURL.index(rawRemoteURL.startIndex, offsetBy: basePrefix.count)
        let rawRemotePath = rawRemoteURL.suffix(from: index)
        let pathComponents = rawRemotePath.split(separator: "/")

        // Get the file name but also take the possible fragment into consideration which is not part of a URL path but a file name.
        // Hence a .lastPathComponent does not work and the path must be split by its slashes.
        guard let fileNameSubstring = pathComponents.last else {
            return nil
        }

        let fileName = String(fileNameSubstring)

        // Derive the parent address by removing the last path component and discarding the fragment which may actually be part of the file name and not a URL fragment.
        let parentPathComponents = pathComponents.dropLast()
        let parentPath = "/\(parentPathComponents.joined(separator: "/"))"
        let rawParentURL = baseURL.absoluteString + parentPath

        return try ItemMetadataRecord
            .filter(ItemMetadataRecord.hasLocation(serverUrl: rawParentURL, fileName: fileName))
            .order(ItemMetadataRecord.Columns.deleted, ItemMetadataRecord.Columns.ocId)
            .fetchOne(db)
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: any ItemMetadata, in db: Database) throws -> ItemMetadataRecord? {
        try self.itemMetadata(account: itemMetadata.account, locatedAtRemoteUrl: itemMetadata.serverUrl, in: db)
    }

    ///
    /// Resolve the parent's "Always keep downloaded" flag for a metadata
    /// that is about to be persisted as a fresh row.
    ///
    /// Mirrors the inheritance applied to locally-created items in
    /// `Item+Create.swift` so a sibling appearing via remote enumeration
    /// acquires the same `contentPolicy` and Finder overlay without the user
    /// having to re-toggle the parent (#10054).
    ///
    /// Checking only the immediate parent is sufficient: the recursive
    /// enable in `Item.set(keepDownloaded:domain:)` sets the flag on every
    /// then-known descendant of the pinned ancestor, so every intermediate
    /// directory between the pin root and this new item is itself pinned.
    ///
    /// Falls back to the root container when no parent row exists at the
    /// item's `serverUrl` — items directly under the user's home are stored
    /// against a synthesised root keyed by ocId, not by serverUrl/fileName.
    ///
    func inheritedKeepDownloaded(for metadata: SendableItemMetadata) -> Bool {
        read("Could not look up the inherited keep-downloaded flag.", [.item: metadata.ocId]) { db in
            try inheritedKeepDownloaded(for: metadata, in: db)
        } ?? false
    }

    func inheritedKeepDownloaded(for metadata: SendableItemMetadata, in db: Database) throws -> Bool {
        if let parent = try parentDirectoryMetadataForItem(metadata, in: db) {
            return parent.keepDownloaded
        }

        if let root = try itemMetadata(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue, in: db) {
            return root.keepDownloaded
        }

        return false
    }

    // MARK: - Writing

    /// Persist `metadata`, evicting any other live row at its logical address first.
    func insertItemMetadata(_ metadata: SendableItemMetadata, in db: Database) throws {
        try evictLogicalDuplicates(of: metadata, in: db)
        try ItemMetadataRecord(metadata).upsertRow(db)
        logger.debug("Added item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl])
    }

    private func processItemMetadatasToDelete(
        existingMetadatas: [ItemMetadataRecord],
        updatedMetadatas: [SendableItemMetadata]
    ) -> [ItemMetadataRecord] {
        let updatedOcIds = Set(updatedMetadatas.map(\.ocId))
        var deletedMetadatas: [ItemMetadataRecord] = []

        for existingMetadata in existingMetadatas where !updatedOcIds.contains(existingMetadata.ocId) {
            deletedMetadatas.append(existingMetadata)

            logger.debug("Deleting item metadata during update.", [.item: existingMetadata.ocId])
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(existingMetadatas: [ItemMetadataRecord], updatedMetadatas: [SendableItemMetadata], keepExistingDownloadState: Bool, in db: Database) throws -> (newMetadatas: [SendableItemMetadata], updatedMetadatas: [SendableItemMetadata], directoriesNeedingRename: [SendableItemMetadata]) {
        var returningNewMetadatas: [SendableItemMetadata] = []
        var returningUpdatedMetadatas: [SendableItemMetadata] = []
        var directoriesNeedingRename: [SendableItemMetadata] = []

        // Keyed once up front; the first occurrence of an identifier wins.
        var existingByOcId: [String: ItemMetadataRecord] = [:]
        existingByOcId.reserveCapacity(existingMetadatas.count)
        for existingMetadata in existingMetadatas where existingByOcId[existingMetadata.ocId] == nil {
            existingByOcId[existingMetadata.ocId] = existingMetadata
        }

        // `inheritedKeepDownloaded` depends on the item only through (account, parent serverUrl); every
        // child of a folder shares one serverUrl, so cache per serverUrl to collapse N parent lookups
        // (each a DB query) to one per distinct parent.
        var inheritedKeepDownloadedByServerUrl: [String: Bool] = [:]

        for var updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingByOcId[updatedMetadata.ocId] {
                if updatedMetadata.etag == existingMetadata.etag {
                    updatedMetadata.fileProviderContentVersion = existingMetadata.fileProviderContentVersion
                }

                if existingMetadata.status == Status.normal.rawValue, !existingMetadata.isInSameDatabaseStoreableRemoteState(updatedMetadata) {
                    let pathChanged = !updatedMetadata.hasSameLocation(as: existingMetadata)

                    if updatedMetadata.directory, pathChanged {
                        directoriesNeedingRename.append(updatedMetadata)
                    }

                    if keepExistingDownloadState {
                        updatedMetadata.downloaded = existingMetadata.downloaded
                    }

                    updatedMetadata.visitedDirectory = existingMetadata.visitedDirectory
                    updatedMetadata.keepDownloaded = existingMetadata.keepDownloaded
                    updatedMetadata.lockToken = pathChanged ? nil : existingMetadata.lockToken

                    returningUpdatedMetadatas.append(updatedMetadata)

                    logger.debug("Updated existing item metadata.", [
                        .item: updatedMetadata.ocId,
                        .eTag: updatedMetadata.etag,
                        .name: updatedMetadata.fileName,
                        .syncTime: updatedMetadata.syncTime.description
                    ])
                } else {
                    logger.debug("Skipping item metadata update; same as existing, or still in transit.", [
                        .item: updatedMetadata.ocId,
                        .eTag: updatedMetadata.etag,
                        .name: updatedMetadata.fileName,
                        .syncTime: updatedMetadata.syncTime.description
                    ])
                }

            } else { // This is a new metadata
                // Inherit the parent's "Always keep downloaded" flag so a file surfacing here via remote enumeration acquires the same pin as its already-pinned siblings (#10054).
                if let cached = inheritedKeepDownloadedByServerUrl[updatedMetadata.serverUrl] {
                    updatedMetadata.keepDownloaded = cached
                } else {
                    let inherited = try inheritedKeepDownloaded(for: updatedMetadata, in: db)
                    inheritedKeepDownloadedByServerUrl[updatedMetadata.serverUrl] = inherited
                    updatedMetadata.keepDownloaded = inherited
                }

                returningNewMetadatas.append(updatedMetadata)

                logger.debug("Created new item metadata during update.", [.item: updatedMetadata.ocId])
            }
        }

        return (returningNewMetadatas, returningUpdatedMetadatas, directoriesNeedingRename)
    }

    /// ONLY HANDLES UPDATES FOR IMMEDIATE CHILDREN
    /// (in case of directory renames/moves, the changes are recursed down)
    ///
    /// Everything, including the renames of moved directories, happens in one transaction.
    public func depth1ReadUpdateItemMetadatas(
        account: String,
        serverUrl: String,
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool
    ) -> ChangeSet? {
        write("Could not update any item metadatas.", [.account: account, .url: serverUrl]) { db in
            // Find the metadatas that we previously knew to be on the server for this account
            // (we need to check if they were uploaded to prevent deleting ignored/lock files)
            //
            // - the ones that do exist remotely still are either the same or have been updated
            // - the ones that don't have been deleted
            var cleanServerUrl = serverUrl
            if cleanServerUrl.last == "/" {
                cleanServerUrl.removeLast()
            }

            let existingMetadatas = try ItemMetadataRecord
                .filter(
                    // Don't worry — root will be updated at the end of this method if is the target
                    ItemMetadataRecord.Columns.ocId != NSFileProviderItemIdentifier.rootContainer.rawValue
                        && ItemMetadataRecord.hasServerUrl(equalTo: cleanServerUrl, includingDescendants: false)
                        && ItemMetadataRecord.Columns.account == account
                        && ItemMetadataRecord.Columns.uploaded == true
                )
                .fetchAll(db)

            var updatedChildMetadatas = updatedMetadatas

            let readTargetMetadata: SendableItemMetadata? = if let targetMetadata = updatedMetadatas.first {
                if targetMetadata.directory {
                    updatedChildMetadatas.removeFirst()
                } else {
                    targetMetadata
                }
            } else {
                nil
            }

            let metadatasToDelete = processItemMetadatasToDelete(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedChildMetadatas
            ).map {
                var metadata = $0.metadata
                metadata.deleted = true
                return metadata
            }

            let metadatasToChange = try processItemMetadatasToUpdate(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedChildMetadatas,
                keepExistingDownloadState: keepExistingDownloadState,
                in: db
            )

            var metadatasToUpdate = metadatasToChange.updatedMetadatas
            var metadatasToCreate = metadatasToChange.newMetadatas
            let directoriesNeedingRename = metadatasToChange.directoriesNeedingRename

            for metadata in directoriesNeedingRename {
                if let updatedDirectoryChildren = try renameDirectoryAndPropagateToChildren(
                    ocId: metadata.ocId,
                    newServerUrl: metadata.serverUrl,
                    newFileName: metadata.fileName,
                    in: db
                ) {
                    metadatasToUpdate += updatedDirectoryChildren
                }
            }

            var visitToRecord: String?

            if var readTargetMetadata {
                if readTargetMetadata.directory {
                    readTargetMetadata.visitedDirectory = true
                }

                if let existing = try itemMetadata(ocId: readTargetMetadata.ocId, in: db) {
                    if readTargetMetadata.etag == existing.etag {
                        readTargetMetadata.fileProviderContentVersion = existing.fileProviderContentVersion
                    }

                    // `visitedDirectory` is local-only, so it takes no part in the remote-state
                    // comparison and is recorded separately from `metadatasToUpdate`, which is also
                    // the change set handed to the framework.
                    if readTargetMetadata.directory, !existing.visitedDirectory {
                        visitToRecord = readTargetMetadata.ocId
                    }

                    if existing.status == Status.normal.rawValue,
                       !existing.isInSameDatabaseStoreableRemoteState(readTargetMetadata)
                    {
                        logger.info("Depth 1 read target changed: \(readTargetMetadata.ocId)")
                        if keepExistingDownloadState {
                            readTargetMetadata.downloaded = existing.downloaded
                        }
                        readTargetMetadata.keepDownloaded = existing.keepDownloaded
                        metadatasToUpdate.insert(readTargetMetadata, at: 0)
                    }
                } else {
                    logger.info("Depth 1 read target is new: \(readTargetMetadata.ocId)")
                    // Inherit from the parent so a directory appearing here via remote enumeration (e.g. created on the server while the user already pinned its parent) picks up the same pin as siblings (#10054).
                    readTargetMetadata.keepDownloaded = try inheritedKeepDownloaded(for: readTargetMetadata, in: db)
                    metadatasToCreate.insert(readTargetMetadata, at: 0)
                }
            }

            // Evict any logical-address duplicates before persisting fresh
            // payloads, so an ocId rotation (or rename whose target collides
            // with a third row) does not leave two non-deleted siblings at
            // the same `(account, serverUrl, fileName)`.
            for metadata in metadatasToCreate {
                try evictLogicalDuplicates(of: metadata, in: db)
            }
            for metadata in metadatasToUpdate {
                try evictLogicalDuplicates(of: metadata, in: db)
            }

            // Do not delete the metadatas that have been deleted
            for metadata in metadatasToDelete + metadatasToUpdate + metadatasToCreate {
                try ItemMetadataRecord(metadata).upsertRow(db)
            }

            if let visitToRecord {
                try ItemMetadataRecord
                    .filter(key: visitToRecord)
                    .updateAll(db, ItemMetadataRecord.Columns.visitedDirectory.set(to: true))
            }

            return ChangeSet(
                created: metadatasToCreate, updated: metadatasToUpdate, deleted: metadatasToDelete
            )
        }
    }

    /// If setting a downloading or uploading status, also modified the relevant boolean properties
    /// of the item metadata object
    public func setStatusForItemMetadata(
        _ metadata: SendableItemMetadata, status: Status
    ) -> SendableItemMetadata? {
        write("Could not update status for item metadata.", [.item: metadata.ocId, .eTag: metadata.etag, .name: metadata.fileName]) { db in
            guard var record = try itemMetadata(ocId: metadata.ocId, in: db) else {
                logger.debug("Did not update status for item metadata as it was not found. ocID: \(metadata.ocId)")
                return nil
            }

            record.status = status.rawValue
            if record.isDownload {
                record.downloaded = false
            } else if record.isUpload {
                record.uploaded = false
            }

            try record.upsertRow(db)

            logger.debug("Updated status for item metadata.", [
                .item: metadata.ocId,
                .eTag: metadata.etag,
                .name: metadata.fileName,
                .syncTime: metadata.syncTime
            ])

            return record.metadata
        } ?? nil
    }

    public func addItemMetadata(_ metadata: SendableItemMetadata) {
        write("Failed to add item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl]) { db in
            try insertItemMetadata(metadata, in: db)
        }
    }

    /// Records that the provider returned `.excludedFromSync` for an item.
    ///
    /// The marker is stored separately from item metadata so remote enumeration and
    /// materialization updates cannot overwrite it before the system calls `deleteItem`.
    ///
    /// - Parameter ocId: The file provider item identifier to mark.
    /// - Returns: `true` when the marker was stored successfully.
    @discardableResult
    func markItemAsExcludedFromSync(ocId: String) -> Bool {
        write("Could not mark item as excluded from sync.", [.item: ocId]) { db in
            try ExcludedFromSyncItemRecord(ocId: ocId).upsert(db)
            return true
        } ?? false
    }

    /// Returns whether an item is awaiting the deletion callback caused by `.excludedFromSync`, or `false` when the database cannot be read.
    /// - Parameter ocId: The file provider item identifier to look up.
    func isItemExcludedFromSync(ocId: String) -> Bool {
        (try? excludedFromSyncMarkerExists(ocId: ocId)) ?? false
    }

    /// Returns whether an item is awaiting the deletion callback caused by `.excludedFromSync`.
    ///
    /// Throws when the database cannot be read, so a caller about to delete on the server can refuse instead of treating an excluded item as an ordinary deletion.
    ///
    func excludedFromSyncMarkerExists(ocId: String) throws -> Bool {
        try writer.read { db in
            try ExcludedFromSyncItemRecord.exists(db, key: ocId)
        }
    }

    /// Removes the durable exclusion marker after local metadata deletion succeeds.
    /// - Parameter ocId: The file provider item identifier whose marker should be removed.
    /// - Returns: `true` when the marker was removed successfully or was already absent.
    @discardableResult
    func removeExcludedFromSyncMarker(ocId: String) -> Bool {
        write("Could not remove excluded-from-sync marker.", [.item: ocId]) { db in
            _ = try ExcludedFromSyncItemRecord.deleteOne(db, key: ocId)
            return true
        } ?? false
    }

    ///
    /// Add or replace `metadata` while carrying over local-only fields the
    /// server payload cannot know about: ``keepDownloaded``, ``downloaded``,
    /// ``visitedDirectory``, ``lockToken``, and the content version File Provider has already seen.
    ///
    /// Mirrors the preservation set applied by
    /// ``processItemMetadatasToUpdate`` for non-paginated reads. Use this from
    /// any code path that ingests fresh PROPFIND results (e.g. paginated
    /// enumeration); plain ``addItemMetadata(_:)`` would otherwise overwrite
    /// these fields back to their defaults, silently undoing user-visible
    /// state such as "Always keep downloaded" (#9923).
    ///
    /// Returns the merged metadata that was persisted. Callers that report
    /// items back to the file-provider framework MUST forward the returned
    /// value rather than the input — otherwise the framework receives the
    /// pre-merge defaults and renders the item as if the local-only state
    /// (e.g. pinned-via-keep-downloaded) had been cleared.
    ///
    /// - Parameters:
    ///   - metadata: The freshly-built metadata to persist.
    ///   - preserveVisitedDirectory: When `false`, do not carry over
    ///     ``visitedDirectory`` from the existing row. Callers that have just
    ///     visited the directory in the current request should pass `false`
    ///     and pre-set `metadata.visitedDirectory = true`, so the visit is
    ///     recorded rather than overwritten by a stale `false` from the DB.
    ///
    @discardableResult
    public func addItemMetadataPreservingLocalState(_ metadata: SendableItemMetadata, preserveVisitedDirectory: Bool = true) -> SendableItemMetadata {
        var toWrite = metadata

        write("Failed to add item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl]) { db in
            if let existing = try itemMetadata(ocId: metadata.ocId, in: db) {
                toWrite.downloaded = existing.downloaded
                toWrite.keepDownloaded = existing.keepDownloaded

                if preserveVisitedDirectory {
                    toWrite.visitedDirectory = existing.visitedDirectory
                }

                toWrite.lockToken = existing.lockToken
                if toWrite.etag == existing.etag {
                    toWrite.fileProviderContentVersion = existing.fileProviderContentVersion
                }
            } else {
                // The ocId lookup missed. Before falling back to defaults from the
                // server payload, look for a single non-deleted, non-local-lock row
                // at the same logical address — an ocId rotation (restore-from-
                // trash, recreate during reconnect, upload finalizer assigning a
                // new server-side ocId) leaves the local-only state on the previous
                // row, and #9923's preservation contract would otherwise silently
                // drop `keepDownloaded`, `downloaded`, `visitedDirectory`, and
                // `lockToken`. Only carry over when exactly one candidate exists:
                // multiple candidates mean the DB is already in the duplicated
                // state and choosing one would risk merging from the row about to
                // be evicted. Eviction in `insertItemMetadata` will then prune the
                // prior row in the same write that persists the fresh one.
                let logicalCandidates = try ItemMetadataRecord
                    .filter(
                        ItemMetadataRecord.hasLocation(serverUrl: metadata.serverUrl, fileName: metadata.fileName)
                            && ItemMetadataRecord.Columns.deleted == false
                            && ItemMetadataRecord.Columns.isLockFileOfLocalOrigin == false
                    )
                    .fetchAll(db)

                if logicalCandidates.count == 1, let existing = logicalCandidates.first {
                    toWrite.downloaded = existing.downloaded
                    toWrite.keepDownloaded = existing.keepDownloaded

                    if preserveVisitedDirectory {
                        toWrite.visitedDirectory = existing.visitedDirectory
                    }

                    toWrite.lockToken = existing.lockToken
                    if toWrite.etag == existing.etag {
                        toWrite.fileProviderContentVersion = existing.fileProviderContentVersion
                    }
                } else {
                    // No prior row at this ocId or logical address: this is a
                    // genuinely new item. Inherit the parent's "Always keep
                    // downloaded" flag so a file surfacing here via remote
                    // enumeration acquires the same pin as its already-pinned
                    // siblings (#10054).
                    toWrite.keepDownloaded = try inheritedKeepDownloaded(for: metadata, in: db)
                }
            }

            try insertItemMetadata(toWrite, in: db)
        }

        return toWrite
    }

    ///
    /// Mark an item as deleted.
    ///
    /// This is a soft delete and does not actually delete data for which there is ``removeItemMetadata(ocId:)``.
    ///
    /// - Parameters:
    ///     - ocId: The unique identifier of the item.
    ///
    @discardableResult public func deleteItemMetadata(ocId: String) -> Bool {
        write("Could not mark item as deleted.", [.item: ocId]) { db in
            try ItemMetadataRecord
                .filter(key: ocId)
                .updateAll(db, ItemMetadataRecord.Columns.deleted.set(to: true))
            logger.debug("Marked item as deleted.", [.item: ocId])
            return true
        } ?? false
    }

    ///
    /// Hard delete an item.
    ///
    /// Unlike ``deleteItemMetadata(ocId:)``, this actually deletes a data record.
    ///
    /// - Parameters:
    ///     - ocId: The unique identifier of the item.
    ///
    public func removeItemMetadata(ocId: String) {
        write("Could not remove item metadata.", [.item: ocId]) { db in
            _ = try ItemMetadataRecord.deleteOne(db, key: ocId)
            logger.debug("Removed item metadata from database.", [.item: ocId])
        }
    }

    public func renameItemMetadata(ocId: String, newServerUrl: String, newFileName: String) {
        write("Could not rename filename of item metadata with ocID: \(ocId) to proposed name \(newFileName) at proposed serverUrl \(newServerUrl).") { db in
            try renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName, in: db)
        }
    }

    func renameItemMetadata(ocId: String, newServerUrl: String, newFileName: String, in db: Database) throws {
        guard var itemMetadata = try itemMetadata(ocId: ocId, in: db) else {
            logger.error("Could not find an item with ocID \(ocId) to rename to \(newFileName)")
            return
        }

        let oldFileName = itemMetadata.fileName
        let oldServerUrl = itemMetadata.serverUrl

        itemMetadata.updateLocation(serverUrl: newServerUrl, fileName: newFileName)
        itemMetadata.fileNameView = newFileName
        itemMetadata.lockToken = nil

        try itemMetadata.upsertRow(db)

        logger.debug("Renamed item \(oldFileName) to \(newFileName), moved from serverUrl: \(oldServerUrl) to serverUrl: \(newServerUrl)")
    }

    public func parentItemIdentifierFromMetadata(
        _ metadata: SendableItemMetadata
    ) -> NSFileProviderItemIdentifier? {
        let homeServerFilesUrl = metadata.urlBase + Account.webDavFilesUrlSuffix + metadata.userId
        let trashServerFilesUrl = metadata.urlBase + Account.webDavTrashUrlSuffix + metadata.userId + "/trash"

        if metadata.serverUrl == homeServerFilesUrl {
            return .rootContainer
        } else if metadata.serverUrl == trashServerFilesUrl {
            return .trashContainer
        }

        guard let parentDirectoryMetadata = parentDirectoryMetadataForItem(metadata) else {
            logger.error("Could not get item parent directory item metadata for metadata.", [.item: metadata.ocId])

            return nil
        }

        return NSFileProviderItemIdentifier(parentDirectoryMetadata.ocId)
    }

    public func parentItemIdentifierWithRemoteFallback(
        fromMetadata metadata: SendableItemMetadata,
        remoteInterface: RemoteInterface,
        account: Account
    ) async -> NSFileProviderItemIdentifier? {
        if let parentItemIdentifier = parentItemIdentifierFromMetadata(metadata) {
            return parentItemIdentifier
        }

        let readResult = await Enumerator.readServerUrl(
            metadata.serverUrl,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: self,
            depth: .target,
            log: logger.log
        )

        guard readResult.error == nil, let parentMetadata = readResult.metadatas?.first else {
            logger.error("Could not retrieve parent item identifier remotely.", [
                .error: readResult.error,
                .item: metadata.ocId,
                .name: metadata.fileName
            ])

            return nil
        }
        return NSFileProviderItemIdentifier(parentMetadata.ocId)
    }

    ///
    /// Return metadata for materialized file provider items.
    ///
    /// - Parameters:
    ///     - account: The account identifier to filter by.
    ///
    /// - Returns: An array of sendable metadata objects.
    ///
    public func materialisedItemMetadatas(account _: String) -> [SendableItemMetadata] {
        read("Could not fetch the materialized item metadata.") { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.isMaterialised)
                .fetchAll(db)
                .map(\.metadata)
        } ?? []
    }

    ///
    /// Look up the not yet synchronized changes and deletions in the materialized items since the last given synchronization time.
    ///
    /// - Parameters:
    ///     - date: All items with a synchronization time later than this are considered.
    ///
    /// - Returns: Locally changed items in the working set grouped by "updated" and "deleted", or `nil` when the database could not be read.
    ///
    public func pendingWorkingSetChanges(since date: Date) -> (updated: [SendableItemMetadata], deleted: [SendableItemMetadata])? {
        logger.debug("Gathering pending working set changes...")

        return read("Could not gather pending working set changes.") { db in
            let pendingChanges = try ItemMetadataRecord
                .filter(ItemMetadataRecord.isMaterialised && ItemMetadataRecord.syncedAfter(date))
                .fetchAll(db)
            var updatedItems = pendingChanges.filter { !$0.deleted }.map(\.metadata)
            var deletedItems = pendingChanges.filter(\.deleted).map(\.metadata)

            for item in updatedItems {
                logger.debug("Found updated item.", [.item: item.ocId, .name: item.fileName])
            }

            for item in deletedItems {
                logger.debug("Found deleted item.", [.item: item.ocId, .name: item.fileName])
            }

            var updatedItemIdentifiers = Set(updatedItems.map(\.ocId))
            var deletedItemIdentifiers = Set(deletedItems.map(\.ocId))

            // Look for changed children
            for serverUrl in updatedItems.filter(\.directory).map({ $0.remotePath() }) {
                let children = try ItemMetadataRecord
                    .filter(
                        ItemMetadataRecord.hasServerUrl(equalTo: serverUrl, includingDescendants: false)
                            && ItemMetadataRecord.syncedAfter(date)
                    )
                    .fetchAll(db)

                for child in children {
                    let sendableMetadata = child.metadata

                    if child.deleted {
                        guard deletedItemIdentifiers.contains(child.ocId) == false else {
                            continue
                        }

                        deletedItemIdentifiers.insert(child.ocId)
                        deletedItems.append(sendableMetadata)
                        logger.debug("Appended deleted item to working set changes.", [.item: child.ocId, .url: serverUrl])
                    } else {
                        guard updatedItemIdentifiers.contains(child.ocId) == false else {
                            continue
                        }

                        updatedItemIdentifiers.insert(child.ocId)
                        updatedItems.append(sendableMetadata)
                        logger.debug("Appended updated item to working set changes.", [.item: child.ocId, .url: serverUrl])
                    }
                }
            }

            // Look for deleted children recursively
            for serverUrl in deletedItems.filter(\.directory).map({ $0.remotePath() }) {
                let children = try ItemMetadataRecord
                    .filter(
                        ItemMetadataRecord.hasServerUrl(equalTo: serverUrl, includingDescendants: true)
                            && ItemMetadataRecord.syncedAfter(date)
                    )
                    .fetchAll(db)

                for child in children {
                    guard child.isLockFileOfLocalOrigin == false else {
                        logger.info("Excluding item from deletion because it is a lock file from local origin.", [.item: child.ocId, .name: child.fileName])
                        continue
                    }

                    guard !deletedItemIdentifiers.contains(child.ocId) else {
                        continue
                    }

                    deletedItemIdentifiers.insert(child.ocId)
                    deletedItems.append(child.metadata)
                    logger.debug("Appended deleted item to working set changes.", [.item: child.ocId, .url: serverUrl])
                }
            }

            return (updatedItems, deletedItems)
        }
    }

    public func itemsMetadataByFileNameSuffix(suffix: String) -> [SendableItemMetadata] {
        logger.debug("Trying to find files matching pattern \"\(suffix)\".")

        let filesMetadata: [SendableItemMetadata] = read("Could not look up files by name suffix.", [.name: suffix]) { db -> [SendableItemMetadata] in
            var request = ItemMetadataRecord.filter(ItemMetadataRecord.Columns.directory == false)

            // An empty suffix matches every file name.
            if !suffix.isEmpty {
                request = request.filter(literal: ItemMetadataRecord.fileNameEnds(with: suffix))
            }

            return try request.order(ItemMetadataRecord.Columns.ocId).fetchAll(db).map(\.metadata)
        } ?? []

        guard !filesMetadata.isEmpty else {
            logger.debug("Could not find files matching pattern \"\(suffix)\".")
            return []
        }

        logger.debug("Found \(filesMetadata.count) file(s) that match \"\(suffix)\" metadata: \(filesMetadata)")

        return filesMetadata
    }
}
