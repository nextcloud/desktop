//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB
import NextcloudKit
import os

///
/// The file provider domain's metadata database.
///
/// Every method enters the database exactly once, through ``read(_:_:)`` or ``write(_:_:)``, and does its work in a worker which takes the open `Database`. Workers call only other workers, because the database access methods are not reentrant. Results leave the database as value types.
///
public final class FilesDatabaseManager: Sendable {
    private enum PendingUploadInDirectoryListing {
        case reportCached(SendableItemMetadata)
        case omitServerItem
    }

    public enum ErrorCode: Int {
        case metadataNotFound = -1000
        case parentMetadataNotFound = -1001
    }

    public enum ErrorUserInfoKey: String {
        case missingParentServerUrlAndFileName = "MissingParentServerUrlAndFileName"
    }

    static let errorDomain = "FilesDatabaseManager"
    static let missingParentChildIdentifierKey = "MissingParentChildIdentifier"

    /// Why the database could not be opened. The domain must not be set up until a later attempt succeeds.
    public enum OpenError: Error {
        /// Neither a custom nor the default support directory is available.
        case noDatabaseDirectory
        /// A Realm database exists but could not be imported; it is left in place for the next attempt.
        case importFailed(URL)
        /// The database file could not be opened or migrated.
        case openFailed(URL, underlying: any Error)
    }

    static func error(code: ErrorCode, userInfo: [String: String]) -> NSError {
        NSError(domain: errorDomain, code: code.rawValue, userInfo: userInfo)
    }

    static func parentMetadataNotFoundError(for metadata: SendableItemMetadata) -> NSError {
        error(
            code: .parentMetadataNotFound,
            userInfo: [
                ErrorUserInfoKey.missingParentServerUrlAndFileName.rawValue: metadata.serverUrl,
                missingParentChildIdentifierKey: metadata.ocId
            ]
        )
    }

    /// Longest list bound into one `IN (...)` clause, well below SQLite's variable limit.
    static let inClauseChunkSize = 500

    ///
    /// How far a write is synchronized to disk before it counts as done.
    ///
    /// Local-only state such as pins, exclusion markers and upload bookkeeping cannot be rebuilt from the server, so writes default to ``full``. Bulk writes of server-derived rows opt into ``relaxed``, which only risks the last commits before a power loss, never corruption.
    ///
    public enum WriteDurability: Sendable {
        /// `PRAGMA synchronous = FULL`: the commit is on disk when the write returns.
        case full
        /// `PRAGMA synchronous = NORMAL`: the commit survives a crash of the process, not necessarily a power loss.
        case relaxed

        var pragma: String {
            switch self {
                case .full: "PRAGMA synchronous = FULL"
                case .relaxed: "PRAGMA synchronous = NORMAL"
            }
        }
    }

    /// Records which package members must stay deleted until the folder is published.
    typealias ActiveDownload = (operation: DownloadOperation, etag: String, remotePath: String, wasDeleted: Bool)

    let logger: FileProviderLogger
    let account: Account
    let syncOperations = SyncOperations()
    /// Share ownership and the starting ETag and path across managers accessing the same database.
    static let downloadOperations = OSAllocatedUnfairLock(initialState: [String: [String: ActiveDownload]]())

    /// The open database: a pool on the domain's file, or an in-memory queue when no directory is available.
    let writer: any DatabaseWriter

    /// The database identifier at the time this manager opened it. Replacing the database or
    /// switching its DAV account changes the identifier and expires previous sync anchors.
    let storeIdentity: String

    /// Location of the database file, `nil` when the database lives in memory.
    let databaseURL: URL?

    /// Downloads the system has not yet reported as materialized. Managers opened by the extension share its registry across account setup.
    let pendingMaterializations: PendingMaterializationRegistry

    init(account: Account, continuingFrom previousManager: FilesDatabaseManager, storeIdentity: String) {
        self.account = account
        self.storeIdentity = storeIdentity
        writer = previousManager.writer
        databaseURL = previousManager.databaseURL
        pendingMaterializations = previousManager.pendingMaterializations
        logger = FileProviderLogger(category: "FilesDatabaseManager", log: previousManager.logger.log)
    }

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
    /// - Throws: ``OpenError`` when no usable database can be opened. The caller must not set the domain up with a substitute, because nothing written to one would survive, and the next attempt may well succeed.
    ///
    public convenience init(
        account: Account,
        databaseDirectory: URL? = nil,
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier,
        log: any FileProviderLogging,
        defaults: UserDefaults = .standard
    ) throws {
        try self.init(
            account: account,
            databaseDirectory: databaseDirectory,
            fileProviderDomainIdentifier: fileProviderDomainIdentifier,
            log: log,
            pendingMaterializations: PendingMaterializationRegistry(),
            defaults: defaults
        )
    }

    init(
        account: Account,
        databaseDirectory customDatabaseDirectory: URL?,
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier,
        log: any FileProviderLogging,
        pendingMaterializations: PendingMaterializationRegistry,
        defaults: UserDefaults = .standard
    ) throws {
        self.account = account
        self.pendingMaterializations = pendingMaterializations
        logger = FileProviderLogger(category: "FilesDatabaseManager", log: log)

        let defaultDatabaseDirectory = FileManager.default.fileProviderDomainSupportDirectory(for: fileProviderDomainIdentifier)

        guard let databaseDirectory = customDatabaseDirectory ?? defaultDatabaseDirectory else {
            logger.fault("Neither custom nor default database directory defined!")
            throw OpenError.noDatabaseDirectory
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

        let previousAccount = domainDefaults.configuredDAVAccountIdentity
        let importOutcome = RealmStoreImporter.importIfNeeded(
            realmURL: realmLocation,
            sqliteURL: databaseLocation,
            account: account,
            previouslyConfiguredAccount: previousAccount,
            logger: logger
        )
        if importOutcome == .failed {
            // The next start retries the import; until then nothing may be written which that import would replace.
            throw OpenError.importFailed(realmLocation)
        }

        (writer, storeIdentity) = try Self.openDatabase(
            at: databaseLocation,
            label: fileProviderDomainIdentifier.rawValue,
            account: account,
            logger: logger
        )
        logger.info("Opened metadata database.", [.url: databaseLocation.path])
        domainDefaults.latestSeenDatabaseVersion = StoreVersion.current

        repairPersistedLogicalAddresses()
    }

    // MARK: - Opening

    static func configuration(label: String) -> Configuration {
        var configuration = Configuration()
        configuration.label = "FilesDatabaseManager.\(label)"
        configuration.busyMode = .timeout(5)
        configuration.maximumReaderCount = 8
        return configuration
    }

    /// Open the file, migrating its schema.
    ///
    /// A file SQLite reports as corrupt is set aside and replaced. Any other failure, such as a busy or full disk, keeps the file untouched and is thrown, so the next start can try again.
    ///
    private static func openDatabase(
        at url: URL,
        label: String,
        account: Account,
        logger: FileProviderLogger
    ) throws -> (writer: any DatabaseWriter, storeIdentity: String) {
        do {
            return try openPool(at: url, label: label, account: account)
        } catch {
            guard isCorruptionError(error) else {
                logger.fault("Could not open the metadata database.", [.url: url.path, .error: error])
                throw OpenError.openFailed(url, underlying: error)
            }

            logger.fault("The metadata database is corrupt. Setting the file aside and starting with an empty one.", [.url: url.path, .error: error])
        }

        setAsideUnreadableDatabase(at: url, logger: logger)

        do {
            return try openPool(at: url, label: label, account: account)
        } catch {
            logger.fault("Could not open a fresh metadata database.", [.url: url.path, .error: error])
            throw OpenError.openFailed(url, underlying: error)
        }
    }

    private static func isCorruptionError(_ error: Error) -> Bool {
        guard let databaseError = error as? DatabaseError else {
            return false
        }

        return [ResultCode.SQLITE_CORRUPT, .SQLITE_NOTADB].contains(databaseError.resultCode.primaryResultCode)
    }

    private static func openPool(
        at url: URL,
        label: String,
        account: Account
    ) throws -> (writer: any DatabaseWriter, storeIdentity: String) {
        let pool = try DatabasePool(path: url.path, configuration: configuration(label: label))
        try DatabaseSchema.migrator.migrate(pool)
        let storeIdentity = try pool.write { db in
            try db.execute(sql: "PRAGMA user_version = \(StoreVersion.current)")
            return try StoreIdentityRecord.updateDatabaseForCurrentDAVAccount(
                account,
                in: db
            )
        }
        return (pool, storeIdentity)
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

    var wasInvalidatedByDAVAccountChange: Bool {
        syncOperations.isInvalidated
    }

    func invalidateForDAVAccountChange() {
        syncOperations.invalidate()
    }

    /// Run a write in one transaction with the given durability; a failure is logged with `failure` and yields `nil`.
    func write<T>(_ failure: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], durability: WriteDurability = .full, _ body: (Database) throws -> T) -> T? {
        do {
            return try write(durability: durability, body)
        } catch {
            var details = details
            details[.error] = error
            logger.error(failure, details)
            return nil
        }
    }

    /// Run a write in one transaction with the given durability, rethrowing a failure.
    ///
    /// The synchronization level is a property of the connection and cannot change inside a transaction, so it is set on the writer right before the transaction opens.
    ///
    func write<T>(durability: WriteDurability, _ body: (Database) throws -> T) throws -> T {
        let operation = SyncOperations.Operation.current
        return try writer.writeWithoutTransaction { db in
            try writeTransaction(in: db, durability: durability, operation: operation, body)
        }
    }

    /// Run a write in one transaction. Cancellation can interrupt the wait for the writer.
    func write<T: Sendable>(
        durability: WriteDurability,
        _ body: @Sendable (Database) throws -> T
    ) async throws -> T {
        let operation = SyncOperations.Operation.current
        return try await writer.writeWithoutTransaction { db in
            try self.writeTransaction(in: db, durability: durability, operation: operation, body)
        }
    }

    private func writeTransaction<T>(
        in db: Database,
        durability: WriteDurability,
        operation: SyncOperations.Operation?,
        _ body: (Database) throws -> T
    ) throws -> T {
        try db.execute(sql: durability.pragma)

        var result: T?
        try db.inTransaction {
            // An account change updates the database identifier and the cached items together.
            // A DAV response queued by the previous manager must not overwrite those rows.
            guard !wasInvalidatedByDAVAccountChange,
                  operation?.isStopped != true,
                  try StoreIdentityRecord.fetchOne(db)?.identity == storeIdentity
            else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            result = try body(db)
            guard !wasInvalidatedByDAVAccountChange, operation?.isStopped != true else {
                throw NSFileProviderError(.cannotSynchronize)
            }
            return .commit
        }

        return result!
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

    ///
    /// Concurrent queue for synchronous database calls made from an `async` context, concurrent
    /// because the pool already serialises writes and runs reads in parallel.
    ///
    private let blockingCallQueue: DispatchQueue = {
        let queue = DispatchQueue(
            label: "com.nextcloud.desktopclient.fileprovider.database", qos: .userInitiated, attributes: .concurrent
        )
        queue.setSpecific(key: blockingCallQueueKey, value: ())
        return queue
    }()

    private static let blockingCallQueueKey = DispatchSpecificKey<Void>()

    /// Whether the caller is running on ``blockingCallQueue``.
    var isOnBlockingCallQueue: Bool {
        DispatchQueue.getSpecific(key: Self.blockingCallQueueKey) != nil
    }

    ///
    /// Run `work` on ``blockingCallQueue``, suspending the caller rather than blocking a thread of the
    /// cooperative pool while SQLite waits for the writer or for a free reader.
    ///
    /// `work` calls the synchronous methods of this type, each of which enters the database on its own, so it must not run inside another database access.
    ///
    public func perform<T: Sendable>(
        _ work: @escaping @Sendable (FilesDatabaseManager) -> T
    ) async -> T {
        let operation = SyncOperations.Operation.current
        return await withCheckedContinuation { continuation in
            blockingCallQueue.async {
                let result = SyncOperations.Operation.$current.withValue(operation) { work(self) }
                continuation.resume(returning: result)
            }
        }
    }

    // MARK: - Lookups

    public func hasItemMetadata() -> Bool {
        read("Could not check whether the domain has item metadata.") { db in
            try !ItemMetadataRecord.all().isEmpty(db)
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
                let matchesFileID = ItemMetadataRecord
                    .filter(chunk.contains(ItemMetadataRecord.Columns.fileId))
                if try !matchesFileID.isEmpty(db) {
                    return true
                }
            }

            return false
        } ?? false
    }

    /// Look up item metadata by its full remote URL in this domain's database.
    public func itemMetadata(locatedAtRemoteUrl rawRemoteURL: String) -> SendableItemMetadata? {
        read("Could not look up item metadata by remote URL.", [.account: account.ncKitAccount, .url: rawRemoteURL]) { db in
            try self.itemMetadata(locatedAtRemoteUrl: rawRemoteURL, in: db)?.metadata
        } ?? nil
    }

    /// Look up the WebDAV Files root for Finder's `.rootContainer` identifier.
    public func rootItemMetadata() -> SendableItemMetadata? {
        read("Could not look up the root item metadata.", [.account: account.ncKitAccount]) { db in
            try fetchFilesRootRecord(in: db)?.metadata
        } ?? nil
    }

    private func fetchFilesRootRecord(in db: Database) throws -> ItemMetadataRecord? {
        try ItemMetadataRecord
            .filter(
                ItemMetadataRecord.Columns.directory
                    && ItemMetadataRecord.isFilesRoot
            )
            .order(ItemMetadataRecord.Columns.deleted, ItemMetadataRecord.Columns.ocId)
            .fetchRecord(db, account: account, logger: logger)
    }

    public func itemMetadatas() -> [SendableItemMetadata] {
        read("Could not fetch the domain's item metadata.") { db in
            try ItemMetadataRecord
                .all()
                .fetchRecords(db, account: account, logger: logger)
                .map(\.metadata)
        } ?? []
    }

    public func itemMetadatas(underServerUrl serverUrl: String) -> [SendableItemMetadata] {
        read("Could not fetch the item metadata under a server URL.", [.url: serverUrl]) { db in
            try ItemMetadataRecord
                .filter(matchesParentDirectoryURL(serverUrl, includingDescendants: true))
                .fetchRecords(db, account: account, logger: logger)
                .map(\.metadata)
        } ?? []
    }

    // MARK: - Lookup workers

    func matchesParentURLAndFileName(parentURL: String, fileName: String) -> SQLExpression {
        guard let parent = account.davPath(forURL: parentURL) else { return false.sqlExpression }
        return ItemMetadataRecord.matchesStoredParentLocationAndFileName(
            davRoot: parent.root,
            parentPath: parent.path,
            fileName: fileName
        )
    }

    func matchesParentDirectoryURL(_ directoryURL: String, includingDescendants: Bool) -> SQLExpression {
        guard let parent = account.davPath(forURL: directoryURL) else { return false.sqlExpression }
        return ItemMetadataRecord.matchesStoredParentLocation(
            davRoot: parent.root,
            parentPath: parent.path,
            includingDescendants: includingDescendants
        )
    }

    func itemMetadata(ocId: String, in db: Database) throws -> ItemMetadataRecord? {
        try ItemMetadataRecord
            .filter(key: ocId)
            .fetchRecord(db, account: account, logger: logger)
    }

    /// The row at a logical address derived from a full remote URL. A live row wins over a deleted one at the same address.
    func itemMetadata(locatedAtRemoteUrl rawRemoteURL: String, in db: Database) throws -> ItemMetadataRecord? {
        if account.davPath(forURL: rawRemoteURL)?.isFilesRoot == true {
            return try fetchFilesRootRecord(in: db)
        }

        // DAV responses contain decoded path components, so "#" and "?" can be part of a name.
        // Split the original URL to preserve the hostname as written and keep those characters in the path.
        guard let schemeSeparator = rawRemoteURL.range(of: "://"),
              let pathStart = rawRemoteURL[schemeSeparator.upperBound...].firstIndex(of: "/"),
              URLComponents(string: String(rawRemoteURL[..<pathStart]))?.host?.isEmpty == false
        else {
            return nil
        }

        let pathComponents = rawRemoteURL[pathStart...].split(separator: "/")
        guard let fileNameComponent = pathComponents.last else { return nil }

        let fileName = String(fileNameComponent)
        let parentEnd = pathComponents.dropLast().last?.endIndex ?? rawRemoteURL.index(after: pathStart)
        let rawParentURL = String(rawRemoteURL[..<parentEnd])

        return try ItemMetadataRecord
            .filter(matchesParentURLAndFileName(parentURL: rawParentURL, fileName: fileName))
            .order(ItemMetadataRecord.Columns.deleted, ItemMetadataRecord.Columns.ocId)
            .fetchRecord(db, account: account, logger: logger)
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: any ItemMetadata, in db: Database) throws -> ItemMetadataRecord? {
        guard !itemMetadata.isFilesRoot else { return nil }

        let parent = try self.itemMetadata(locatedAtRemoteUrl: itemMetadata.serverUrl, in: db)
        // Finder stores the root's pin on the synthetic root row.
        if parent?.isFilesRootWithServerID == true,
           let syntheticRoot = try self.itemMetadata(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue, in: db),
           syntheticRoot.directory,
           !syntheticRoot.deleted
        {
            return syntheticRoot
        }

        return parent
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
    /// Falls back to the synthetic root's pin when the parent row is missing.
    ///
    func inheritedKeepDownloaded(for metadata: SendableItemMetadata, in db: Database) throws -> Bool {
        guard !metadata.isFilesRoot else { return false }

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
        let record = try ItemMetadataRecord(metadata, domainAccount: account)
        try evictLogicalDuplicates(of: record, in: db)
        try record.upsertRow(db)
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

    private func processItemMetadatasToUpdate(
        existingMetadatas: [ItemMetadataRecord],
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool,
        requiresCompleteListing: Bool,
        in db: Database
    ) throws -> (
        newMetadatas: [SendableItemMetadata],
        updatedMetadatas: [SendableItemMetadata],
        directoriesNeedingRename: [SendableItemMetadata],
        itemMetadataToReportByID: [String: SendableItemMetadata],
        itemIDsToOmitFromListing: Set<String>
    ) {
        var returningNewMetadatas: [SendableItemMetadata] = []
        var returningUpdatedMetadatas: [SendableItemMetadata] = []
        var directoriesNeedingRename: [SendableItemMetadata] = []
        var itemMetadataToReportByID: [String: SendableItemMetadata] = [:]
        var itemIDsToOmitFromListing: Set<String> = []

        // Keyed once up front; the first occurrence of an identifier wins.
        var existingByOcId: [String: ItemMetadataRecord] = [:]
        existingByOcId.reserveCapacity(existingMetadatas.count)
        for existingMetadata in existingMetadatas where existingByOcId[existingMetadata.ocId] == nil {
            existingByOcId[existingMetadata.ocId] = existingMetadata
        }

        // `inheritedKeepDownloaded` depends on the parent URL; every
        // child of a folder shares one serverUrl, so cache per serverUrl to collapse N parent lookups
        // (each a DB query) to one per distinct parent.
        var inheritedKeepDownloadedByServerUrl: [String: Bool] = [:]

        for var updatedMetadata in updatedMetadatas {
            let existing = existingByOcId[updatedMetadata.ocId]
            let cachedItem = try existing ?? itemMetadata(ocId: updatedMetadata.ocId, in: db)
            if let pendingUpload = pendingUploadInDirectoryListing(
                updatedMetadata,
                cachedItem: cachedItem,
                requireExactCachedPath: requiresCompleteListing
            ) {
                switch pendingUpload {
                    case .reportCached(let metadata):
                        itemMetadataToReportByID[metadata.ocId] = metadata
                    case .omitServerItem:
                        itemIDsToOmitFromListing.insert(updatedMetadata.ocId)
                }
                continue
            }

            if let existingMetadata = existing {
                let canRefreshFromServer = existingMetadata.status == Status.normal.rawValue || (existingMetadata.status == Status.uploadError.rawValue && existingMetadata.uploaded)
                let sameRemoteState = existingMetadata.isInSameDatabaseStoreableRemoteState(updatedMetadata)
                if canRefreshFromServer, !sameRemoteState {
                    let pathChanged = !updatedMetadata.hasSameLocation(as: existingMetadata)

                    if updatedMetadata.directory, pathChanged {
                        directoriesNeedingRename.append(updatedMetadata)
                    }

                    mergeLocalFields(
                        into: &updatedMetadata,
                        from: existingMetadata,
                        preserveDownloaded: keepExistingDownloadState,
                        lockToken: pathChanged ? nil : existingMetadata.lockToken
                    )

                    returningUpdatedMetadatas.append(updatedMetadata)
                    itemMetadataToReportByID[updatedMetadata.ocId] = updatedMetadata

                    logger.debug("Updated existing item metadata.", [
                        .item: updatedMetadata.ocId,
                        .eTag: updatedMetadata.etag,
                        .name: updatedMetadata.fileName,
                        .syncTime: updatedMetadata.syncTime.description
                    ])
                } else {
                    if sameRemoteState, !existingMetadata.deleted, existingMetadata.hasSameLocation(as: updatedMetadata) {
                        itemMetadataToReportByID[updatedMetadata.ocId] = existingMetadata.metadata
                    }
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

        // Save distinct server rows even if a pending edit keeps the name in this listing.
        let pendingUploadNames = Set(itemMetadataToReportByID.values.lazy.filter(\.hasUnfinishedUpload).map(\.fileName))
        itemIDsToOmitFromListing.formUnion(
            updatedMetadatas.lazy.filter {
                itemMetadataToReportByID[$0.ocId]?.hasUnfinishedUpload != true && pendingUploadNames.contains($0.fileName)
            }.map(\.ocId)
        )

        return (
            returningNewMetadatas,
            returningUpdatedMetadatas,
            directoriesNeedingRename,
            itemMetadataToReportByID,
            itemIDsToOmitFromListing
        )
    }

    /// Report a pending edit under its cached name while it still belongs to the listed folder.
    private func pendingUploadInDirectoryListing(
        _ listed: SendableItemMetadata,
        cachedItem: ItemMetadataRecord?,
        requireExactCachedPath: Bool = false
    ) -> PendingUploadInDirectoryListing? {
        guard let cached = cachedItem, !cached.deleted, cached.hasUnfinishedUpload else { return nil }

        let isInListedFolder = cached.serverUrl == listed.serverUrl || account.hasSameDAVPath(cached.serverUrl, listed.serverUrl)
        let canReportCachedPath = !requireExactCachedPath || cached.hasSameLocation(as: listed)
        guard cached.directory == listed.directory,
              isInListedFolder,
              canReportCachedPath
        else { return .omitServerItem }
        return .reportCached(cached.metadata)
    }

    private func pendingUploadMetadataMatchingListingItem(
        _ existing: ItemMetadataRecord?,
        listed: SendableItemMetadata
    ) throws -> SendableItemMetadata? {
        switch pendingUploadInDirectoryListing(listed, cachedItem: existing, requireExactCachedPath: true) {
            case .reportCached(let metadata): return metadata
            case .omitServerItem: throw NKError.invalidData
            case nil: return nil
        }
    }

    /// ONLY HANDLES UPDATES FOR IMMEDIATE CHILDREN
    /// (in case of directory renames/moves, the changes are recursed down)
    ///
    /// Everything, including the renames of moved directories, happens in one transaction. The rows are server-derived, so the write is relaxed.
    public func depth1ReadUpdateItemMetadatas(
        serverUrl: String,
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool,
        preserveDeletedTarget: Bool = false
    ) -> ChangeSet? {
        saveDepth1ListingMetadata(
            serverUrl: serverUrl,
            updatedMetadatas: updatedMetadatas,
            keepExistingDownloadState: keepExistingDownloadState,
            preserveDeletedTarget: preserveDeletedTarget
        )?.changes
    }

    /// Save the listing and return the requested folder's metadata with its local flags.
    /// Return replacement metadata or omit entries as needed to keep pending uploads at their cached paths.
    func saveDepth1ListingMetadata(
        serverUrl: String,
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool,
        preserveDeletedTarget: Bool,
        requiresCompleteListing: Bool = false
    ) -> (
        changes: ChangeSet,
        itemMetadataToReportByID: [String: SendableItemMetadata],
        itemIDsToOmitFromListing: Set<String>
    )? {
        write("Could not update any item metadatas.", [.url: serverUrl], durability: .relaxed) { db in
            // A listing can include the server's older copy of a pending upload.
            // Include the pending rows so the merge keeps the local edits.
            var cleanServerUrl = serverUrl
            if cleanServerUrl.last == "/" {
                cleanServerUrl.removeLast()
            }

            let existingMetadatas = try ItemMetadataRecord
                .filter(
                    // The root can share its stored parent with its own children.
                    !ItemMetadataRecord.isFilesRoot
                        && matchesParentDirectoryURL(cleanServerUrl, includingDescendants: false)
                        && (ItemMetadataRecord.Columns.uploaded || ItemMetadataRecord.hasUnfinishedUpload)
                )
                .fetchRecords(db, account: account, logger: logger)

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
                existingMetadatas: existingMetadatas.filter { $0.uploaded && !$0.hasUnfinishedUpload },
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
                requiresCompleteListing: requiresCompleteListing,
                in: db
            )
            // A folder download publishes the contents as a unit. It cannot skip a pending edit.
            if requiresCompleteListing, !metadatasToChange.itemIDsToOmitFromListing.isEmpty {
                throw NKError.invalidData
            }

            var metadatasToUpdate = metadatasToChange.updatedMetadatas
            var metadatasToCreate = metadatasToChange.newMetadatas
            var itemMetadataToReportByID = metadatasToChange.itemMetadataToReportByID
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

                let existing = try itemMetadata(ocId: readTargetMetadata.ocId, in: db)
                let pendingUpload = try pendingUploadMetadataMatchingListingItem(
                    existing,
                    listed: readTargetMetadata
                )
                if var pendingUpload {
                    if pendingUpload.directory, !pendingUpload.visitedDirectory {
                        visitToRecord = pendingUpload.ocId
                        pendingUpload.visitedDirectory = true
                    }
                    readTargetMetadata = pendingUpload
                } else if let existing {
                    // A folder fetch publishes a deleted directory only after its downloads succeed.
                    let canUpdateTarget = !preserveDeletedTarget || !existing.deleted
                    if keepExistingDownloadState {
                        readTargetMetadata.downloaded = existing.downloaded
                    }
                    readTargetMetadata.keepDownloaded = existing.keepDownloaded
                    if readTargetMetadata.etag == existing.etag {
                        readTargetMetadata.fileProviderContentVersion = existing.fileProviderContentVersion
                    }

                    // `visitedDirectory` is local-only, so it takes no part in the remote-state
                    // comparison and is recorded separately from `metadatasToUpdate`, which is also
                    // the change set handed to the framework.
                    if canUpdateTarget, readTargetMetadata.directory, !existing.visitedDirectory {
                        visitToRecord = readTargetMetadata.ocId
                    }

                    if canUpdateTarget,
                       existing.status == Status.normal.rawValue,
                       !existing.isInSameDatabaseStoreableRemoteState(readTargetMetadata)
                    {
                        logger.info("Depth 1 read target changed: \(readTargetMetadata.ocId)")
                        metadatasToUpdate.insert(readTargetMetadata, at: 0)
                    }
                } else {
                    logger.info("Depth 1 read target is new: \(readTargetMetadata.ocId)")
                    // Inherit from the parent so a directory appearing here via remote enumeration (e.g. created on the server while the user already pinned its parent) picks up the same pin as siblings (#10054).
                    readTargetMetadata.keepDownloaded = try inheritedKeepDownloaded(for: readTargetMetadata, in: db)
                    metadatasToCreate.insert(readTargetMetadata, at: 0)
                }
                itemMetadataToReportByID[readTargetMetadata.ocId] = readTargetMetadata
            }

            // Evict any logical-address duplicates before persisting fresh
            // payloads, so an ocId rotation (or rename whose target collides
            // with a third row) does not leave two non-deleted siblings at
            // the same parent path and name.
            let recordsToCreate = try metadatasToCreate.map {
                try ItemMetadataRecord($0, domainAccount: account)
            }
            let recordsToUpdate = try metadatasToUpdate.map {
                try ItemMetadataRecord($0, domainAccount: account)
            }
            for record in recordsToCreate + recordsToUpdate {
                try evictLogicalDuplicates(of: record, in: db)
            }

            // Do not delete the metadatas that have been deleted
            for metadata in metadatasToDelete {
                try ItemMetadataRecord(metadata, domainAccount: self.account).upsertRow(db)
            }
            for record in recordsToUpdate + recordsToCreate {
                try record.upsertRow(db)
            }

            if let visitToRecord {
                try ItemMetadataRecord
                    .filter(key: visitToRecord)
                    .updateAll(db, ItemMetadataRecord.Columns.visitedDirectory.set(to: true))
            }

            let changes = ChangeSet(
                created: metadatasToCreate,
                updated: metadatasToUpdate,
                deleted: metadatasToDelete
            )
            return (changes, itemMetadataToReportByID, metadatasToChange.itemIDsToOmitFromListing)
        }
    }

    /// If setting a downloading or uploading status, also modified the relevant boolean properties
    /// of the item metadata object
    public func setStatusForItemMetadata(
        _ metadata: SendableItemMetadata,
        status: Status
    ) -> SendableItemMetadata? {
        do {
            return try setStatusForItemMetadata(ocId: metadata.ocId, status: status)
        } catch {
            logger.error("Could not update status for item metadata.", [
                .item: metadata.ocId,
                .eTag: metadata.etag,
                .name: metadata.fileName,
                .error: error
            ])
            return nil
        }
    }

    /// Report database failures separately from a missing item in the current account.
    func setStatusForItemMetadata(ocId: String, status: Status) throws -> SendableItemMetadata? {
        try write(durability: .full) { db in
            guard var record = try itemMetadata(ocId: ocId, in: db), !record.deleted else { return nil }
            try record.updateChanges(db) {
                $0.status = status.rawValue
                if $0.isDownload {
                    $0.downloaded = false
                } else if $0.isUpload {
                    $0.uploaded = false
                }
            }
            return record.metadata
        }
    }

    /// Save the upload attempt ID in the existing session field before sending the edit.
    func beginEditedItemUpload(ocId: String, uploadAttemptID: String) throws -> SendableItemMetadata? {
        try write(durability: .full) { db in
            guard var record = try itemMetadata(ocId: ocId, in: db), !record.deleted else { return nil }
            try record.updateChanges(db) {
                $0.status = Status.uploading.rawValue
                $0.uploaded = false
                $0.session = uploadAttemptID
            }
            return record.metadata
        }
    }

    /// Save the server result for the exact upload attempt that sent the edit.
    func completeEditedItemUpload(
        ocId: String,
        uploadAttemptID: String,
        etag: String?,
        date: Date?,
        size: Int64?
    ) throws -> SendableItemMetadata {
        try write(durability: .full) { db in
            guard var item = try itemMetadata(ocId: ocId, in: db),
                  !item.deleted,
                  item.status == Status.uploading.rawValue,
                  item.session == uploadAttemptID
            else { throw NSFileProviderError(.cannotSynchronize) }

            try item.updateChanges(db) {
                $0.status = Status.normal.rawValue
                $0.date = date ?? $0.date
                $0.etag = etag ?? $0.etag
                $0.fileProviderContentVersion = $0.etag
                $0.size = size ?? 0
                $0.session = ""
                $0.sessionError = ""
                $0.sessionTaskIdentifier = 0
                $0.downloaded = true
                $0.uploaded = true
            }
            return item.metadata
        }
    }

    /// Update the current upload state without writing back metadata captured before the transfer.
    func recordUploadFailure(
        ocId: String,
        uploadAttemptID: String,
        errorDescription: String,
        staleLockToken: String?,
        serverVersionConflict: Bool
    ) {
        write("Could not record upload failure.", [.item: ocId]) { db in
            guard var record = try itemMetadata(ocId: ocId, in: db),
                  !record.deleted,
                  record.session == uploadAttemptID,
                  record.status == Status.uploading.rawValue
            else { return }

            try record.updateChanges(db) {
                $0.status = Status.uploadError.rawValue
                $0.session = ""
                $0.sessionError = errorDescription
                if serverVersionConflict {
                    // File Provider keeps the rejected edit. The row can describe the server copy again.
                    $0.uploaded = true
                }
                if let staleLockToken, $0.lockToken == staleLockToken {
                    $0.lockToken = nil
                }
            }
        }
    }

    /// Persist `metadata`, evicting any other live row at its logical address first.
    ///
    /// - Parameter durability: ``WriteDurability/full`` unless the row only mirrors the server.
    ///
    public func addItemMetadata(_ metadata: SendableItemMetadata, durability: WriteDurability = .full) {
        write("Failed to add item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl], durability: durability) { db in
            try insertItemMetadata(metadata, in: db)
        }
    }

    /// Save a single-item DAV response with the local fields current at the time of the write.
    /// Return the merged metadata and whether it was newly added. Leave pending uploads intact.
    /// Roll back if cancellation is detected before commit.
    func addItemMetadataFromRemoteRead(
        _ metadata: SendableItemMetadata,
        requestedURL: String,
        isCancelled: @Sendable @escaping () -> Bool
    ) async throws -> (metadata: SendableItemMetadata, isNew: Bool) {
        try await write(durability: .full) { db in
            guard !isCancelled() else { throw CancellationError() }
            let currentItem = try self.itemMetadata(ocId: metadata.ocId, in: db)
            if let pendingUpload = try self.pendingUploadMetadataMatchingListingItem(
                currentItem,
                listed: metadata
            ) {
                guard self.account.hasSameDAVPath(metadata.remotePath(), requestedURL) else { throw NKError.invalidData }
                return (pendingUpload, false)
            }

            let merged = try self.mergeLocalFieldsIntoRemoteRead(metadata, currentItem: currentItem, in: db)
            try self.insertItemMetadata(merged.metadata, in: db)
            // Progress can be cancelled before its handler cancels the Swift task.
            guard !isCancelled() else { throw CancellationError() }
            return merged
        }
    }

    /// Read the local fields when a single-item DAV response can be returned despite a write failure.
    func itemMetadataFromRemoteReadWithoutSaving(
        _ metadata: SendableItemMetadata
    ) -> (metadata: SendableItemMetadata, isNew: Bool) {
        read("Could not read local fields for a remote item.", [.item: metadata.ocId]) { db in
            let currentItem = try itemMetadata(ocId: metadata.ocId, in: db)
            return try mergeLocalFieldsIntoRemoteRead(metadata, currentItem: currentItem, in: db)
        } ?? (metadata, true)
    }

    private func mergeLocalFieldsIntoRemoteRead(
        _ metadata: SendableItemMetadata,
        currentItem: ItemMetadataRecord?,
        in db: Database
    ) throws -> (metadata: SendableItemMetadata, isNew: Bool) {
        var merged = metadata
        if let currentItem {
            mergeLocalFields(
                into: &merged,
                from: currentItem,
                lockToken: currentItem.hasSameLocation(as: metadata) ? currentItem.lockToken : nil
            )
        } else {
            merged.keepDownloaded = try inheritedKeepDownloaded(for: metadata, in: db)
        }
        return (merged, currentItem == nil)
    }

    private func mergeLocalFields(
        into metadata: inout SendableItemMetadata,
        from cached: ItemMetadataRecord,
        preserveDownloaded: Bool = true,
        preserveVisitedDirectory: Bool = true,
        lockToken: String?
    ) {
        if preserveDownloaded {
            metadata.downloaded = cached.downloaded
        }
        if preserveVisitedDirectory {
            metadata.visitedDirectory = cached.visitedDirectory
        }
        metadata.keepDownloaded = cached.keepDownloaded
        metadata.lockToken = lockToken
        metadata.fileProviderContentVersion = metadata.etag == cached.etag ? cached.fileProviderContentVersion : nil
    }

    /// Set the given `downloaded` and `visitedDirectory` values of several rows in one write transaction, leaving every other column and every missing row untouched; a failure discards the whole batch.
    ///
    /// - Parameter durability: ``WriteDurability/full`` unless the flags only mirror the system's materialized set, which the next reconciliation pass rebuilds.
    ///
    func setMaterializationStates(_ states: [(ocId: String, downloaded: Bool?, visitedDirectory: Bool?)], durability: WriteDurability = .full) {
        guard !states.isEmpty else { return }

        write("Failed to update the materialization state of item metadata records.", durability: durability) { db in
            for state in states {
                var assignments = [ColumnAssignment]()

                if let downloaded = state.downloaded {
                    assignments.append(ItemMetadataRecord.Columns.downloaded.set(to: downloaded))
                }

                if let visitedDirectory = state.visitedDirectory {
                    assignments.append(ItemMetadataRecord.Columns.visitedDirectory.set(to: visitedDirectory))
                }

                guard !assignments.isEmpty else { continue }

                try ItemMetadataRecord
                    .filter(key: state.ocId)
                    .updateAll(db, assignments)
            }
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
    /// Returns the metadata to report to File Provider, including the existing
    /// row when the item has an unfinished upload. Callers must report this
    /// value so File Provider sees local state such as ``keepDownloaded``.
    /// Incoming server metadata uses defaults for these local-only fields.
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
        do {
            return try persistItemMetadataFromPagedListing(metadata, preserveVisitedDirectory: preserveVisitedDirectory) ?? metadata
        } catch {
            logger.error("Failed to add item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl, .error: error])
            return metadata
        }
    }

    /// Return nil for a listed child whose server entry would hide a pending local edit.
    func persistItemMetadataFromPagedListing(
        _ metadata: SendableItemMetadata,
        preserveVisitedDirectory: Bool = true,
        isChildInDirectoryListing: Bool = false
    ) throws -> SendableItemMetadata? {
        // The local flags carried over from the old row were committed durably when set.
        try write(durability: .relaxed) { db in
            var toWrite = metadata
            let currentItem = try itemMetadata(ocId: metadata.ocId, in: db)
            if isChildInDirectoryListing {
                if let pendingUpload = pendingUploadInDirectoryListing(metadata, cachedItem: currentItem) {
                    switch pendingUpload {
                        case .reportCached(let cached): return cached
                        case .omitServerItem: return nil
                    }
                }
            } else if let pendingUpload = try pendingUploadMetadataMatchingListingItem(currentItem, listed: metadata) {
                return pendingUpload
            }

            // The renamed item can be on another page. Its pending edit keeps the cached
            // name in Finder, even as we save the distinct server row at that path.
            let omitFromListing = try isChildInDirectoryListing && !ItemMetadataRecord
                .filter(matchesParentURLAndFileName(parentURL: metadata.serverUrl, fileName: metadata.fileName))
                .filter(!ItemMetadataRecord.Columns.deleted)
                .filter(ItemMetadataRecord.hasUnfinishedUpload)
                .isEmpty(db)

            if let existing = currentItem {
                mergeLocalFields(
                    into: &toWrite,
                    from: existing,
                    preserveVisitedDirectory: preserveVisitedDirectory,
                    lockToken: existing.lockToken
                )

                // An ocId lookup can find a moved or deleted row, so check location and deletion state.
                // Compare visitedDirectory separately because it is local-only.
                // Writing other non-normal rows resets their state.
                if !existing.deleted,
                   existing.status == Status.normal.rawValue,
                   existing.hasSameLocation(as: toWrite),
                   existing.isInSameDatabaseStoreableRemoteState(toWrite),
                   existing.visitedDirectory == toWrite.visitedDirectory
                {
                    logger.debug("Skipping item metadata write; database already holds this remote state.", [.item: toWrite.ocId, .name: toWrite.fileName])
                    return omitFromListing ? nil : toWrite
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
                        matchesParentURLAndFileName(parentURL: metadata.serverUrl, fileName: metadata.fileName)
                            && ItemMetadataRecord.Columns.deleted == false
                            && ItemMetadataRecord.Columns.isLockFileOfLocalOrigin == false
                    )
                    .fetchRecords(db, account: account, logger: logger)

                if !omitFromListing, logicalCandidates.count == 1, let existing = logicalCandidates.first {
                    mergeLocalFields(
                        into: &toWrite,
                        from: existing,
                        preserveVisitedDirectory: preserveVisitedDirectory,
                        lockToken: existing.lockToken
                    )
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
            return omitFromListing ? nil : toWrite
        }
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
        deleteItemMetadata(ocId: ocId, syncTime: nil)
    }

    /// Record the deletion and, for a working-set scan, when it was discovered.
    func deleteItemMetadata(ocId: String, syncTime: Date?) -> Bool {
        write("Could not mark item as deleted.", [.item: ocId]) { db in
            var updates = [ItemMetadataRecord.Columns.deleted.set(to: true)]
            if let syncTime {
                updates.append(ItemMetadataRecord.Columns.syncTime.set(to: syncTime.timeIntervalSinceReferenceDate))
            }
            try ItemMetadataRecord.filter(key: ocId).updateAll(db, updates)
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

        try itemMetadata.updateLocation(parentURL: newServerUrl, fileName: newFileName, domainAccount: account)
        itemMetadata.fileNameView = newFileName
        itemMetadata.lockToken = nil

        try itemMetadata.upsertRow(db)

        logger.debug("Renamed item \(oldFileName) to \(newFileName), moved from serverUrl: \(oldServerUrl) to serverUrl: \(newServerUrl)")
    }

    public func parentItemIdentifierFromMetadata(
        _ metadata: SendableItemMetadata
    ) -> NSFileProviderItemIdentifier? {
        if let parent = account.davPath(forURL: metadata.serverUrl), parent.isRoot {
            return parent.root == .files ? .rootContainer : .trashContainer
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
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in },
        isCancelled: @Sendable @escaping () -> Bool = { Task.isCancelled }
    ) async -> NSFileProviderItemIdentifier? {
        guard !isCancelled() else { return nil }
        if let parentItemIdentifier = parentItemIdentifierFromMetadata(metadata) {
            return parentItemIdentifier
        }

        let readResult = await Enumerator.readServerUrl(
            metadata.serverUrl,
            sourceMetadata: metadata,
            remoteInterface: remoteInterface,
            dbManager: self,
            depth: .target,
            taskHandler: taskHandler,
            isCancelled: isCancelled,
            log: logger.log
        )

        guard !isCancelled() else { return nil }

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

    /// Return metadata for this domain's materialized items.
    ///
    /// Returns an empty array when no items are materialized and `nil` when the database read fails.
    public func materialisedItemMetadatas() -> [SendableItemMetadata]? {
        read("Could not fetch the materialized item metadata.") { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.isMaterialised)
                .fetchRecords(db, account: account, logger: logger)
                .map(\.metadata)
        }
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
                .filter(
                    ItemMetadataRecord.isMaterialised
                        && ItemMetadataRecord.syncedAfter(date)
                )
                .fetchRecords(db, account: account, logger: logger)
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
                        matchesParentDirectoryURL(serverUrl, includingDescendants: false)
                            && ItemMetadataRecord.syncedAfter(date)
                    )
                    .fetchRecords(db, account: account, logger: logger)

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
                        matchesParentDirectoryURL(serverUrl, includingDescendants: true)
                            && ItemMetadataRecord.syncedAfter(date)
                    )
                    .fetchRecords(db, account: account, logger: logger)

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
            var request = ItemMetadataRecord
                .filter(ItemMetadataRecord.Columns.directory == false)

            // An empty suffix matches every file name.
            if !suffix.isEmpty {
                request = request.filter(literal: ItemMetadataRecord.fileNameEnds(with: suffix))
            }

            return try request.order(ItemMetadataRecord.Columns.ocId).fetchRecords(db, account: account, logger: logger).map(\.metadata)
        } ?? []

        guard !filesMetadata.isEmpty else {
            logger.debug("Could not find files matching pattern \"\(suffix)\".")
            return []
        }

        logger.debug("Found \(filesMetadata.count) file(s) that match \"\(suffix)\" metadata: \(filesMetadata)")

        return filesMetadata
    }
}
