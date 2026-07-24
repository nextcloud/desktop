//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import RealmSwift

///
/// Realm database abstraction and management.
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

    private static let schemaVersion = SchemaVersion.addedCanonicalPathKeysToRealmItemMetadata
    let logger: FileProviderLogger
    let account: Account

    var itemMetadatas: Results<RealmItemMetadata> {
        ncDatabase().objects(RealmItemMetadata.self)
    }

    ///
    /// Convenience initializer which defines a default configuration for Realm.
    ///
    /// - Parameters:
    ///     - customConfiguration: Optional custom Realm configuration to use instead of the default one.
    ///     - account: The Nextcloud account for which the database is being created.
    ///     - customDatabaseDirectory: Optional custom directory where the database files should be stored. If not provided, the default directory will be used.
    ///
    public init(realmConfiguration customConfiguration: Realm.Configuration? = nil, account: Account, databaseDirectory customDatabaseDirectory: URL? = nil, fileProviderDomainIdentifier: NSFileProviderDomainIdentifier, log: any FileProviderLogging) {
        self.account = account
        logger = FileProviderLogger(category: "FilesDatabaseManager", log: log)

        let defaultDatabaseDirectory = FileManager.default.fileProviderDomainSupportDirectory(for: fileProviderDomainIdentifier)

        guard let databaseDirectory = customDatabaseDirectory ?? defaultDatabaseDirectory else {
            logger.fault("Neither custom nor default database directory defined!")
            return
        }

        let databaseLocation = databaseDirectory
            .appendingPathComponent(fileProviderDomainIdentifier.rawValue)
            .appendingPathExtension("realm")

        let configuration = customConfiguration ?? Realm.Configuration(
            fileURL: databaseLocation,
            schemaVersion: Self.schemaVersion.rawValue,
            migrationBlock: { migration, oldSchemaVersion in
                if oldSchemaVersion == SchemaVersion.initial.rawValue {
                    var localFileMetadataOcIds = Set<String>()

                    migration.enumerateObjects(ofType: "LocalFileMetadata") { oldObject, _ in
                        guard let oldObject, let lfmOcId = oldObject["ocId"] as? String else {
                            return
                        }

                        localFileMetadataOcIds.insert(lfmOcId)
                    }

                    migration.enumerateObjects(ofType: RealmItemMetadata.className()) { _, newObject in
                        guard let newObject,
                              let imOcId = newObject["ocId"] as? String,
                              localFileMetadataOcIds.contains(imOcId)
                        else { return }

                        newObject["downloaded"] = true
                        newObject["uploaded"] = true
                    }
                }

                if oldSchemaVersion < SchemaVersion.addedCanonicalPathKeysToRealmItemMetadata.rawValue {
                    migration.enumerateObjects(ofType: RealmItemMetadata.className()) { _, newObject in
                        guard let newObject,
                              let serverUrl = newObject["serverUrl"] as? String,
                              let fileName = newObject["fileName"] as? String
                        else { return }

                        newObject["normalizedServerUrl"] = serverUrl.canonicalForm
                        newObject["normalizedFileName"] = fileName.canonicalForm
                    }
                }
            },
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )

        Realm.Configuration.defaultConfiguration = configuration

        do {
            _ = try Realm()
            logger.info("Successfully created Realm.")
            cleanupPreexistingLogicalDuplicates()
        } catch {
            logger.fault("Error creating Realm: \(error)")
        }
    }

    func ncDatabase() -> Realm {
        let realm = try! Realm()
        realm.refresh()
        return realm
    }

    public func anyItemMetadatasForAccount(_ account: String) -> Bool {
        !itemMetadatas.where { $0.account == account }.isEmpty
    }

    public func itemMetadata(ocId: String) -> SendableItemMetadata? {
        // Realm objects are live-fire, i.e. they will be changed and invalidated according to
        // changes in the db.
        //
        // Let's therefore create a copy
        if let itemMetadata = itemMetadatas.where({ $0.ocId == ocId }).first {
            return SendableItemMetadata(value: itemMetadata)
        }

        return nil
    }

    public func itemMetadata(_ identifier: NSFileProviderItemIdentifier) -> SendableItemMetadata? {
        itemMetadata(ocId: identifier.rawValue)
    }

    /// Return whether metadata exists for any numeric WebDAV file ID received from notify-push.
    public func containsAnyItemMetadata(fileIds: Set<String>) -> Bool {
        itemMetadatas.contains { fileIds.contains($0.fileId) }
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

        if let metadata = itemMetadatas.where({ item in
            RealmItemMetadata.hasLocation(
                item,
                account: account,
                serverUrl: rawParentURL,
                fileName: fileName
            )
        }).first {
            return SendableItemMetadata(value: metadata)
        }

        return nil
    }

    ///
    /// Fetch the metadata object for the root container of the given account.
    ///
    /// This is useful for when you have only the `NSFileProviderItemIdentifier.rootContainer` but no `ocId` to look up metadata by.
    ///
    public func rootItemMetadata(account: Account) -> SendableItemMetadata? {
        guard let object = itemMetadatas.where({ $0.account == account.ncKitAccount && $0.directory && $0.path == Account.webDavFilesUrlSuffix }).first else {
            return nil
        }

        return SendableItemMetadata(value: object)
    }

    public func itemMetadatas(account: String) -> [SendableItemMetadata] {
        itemMetadatas
            .where { $0.account == account }
            .toUnmanagedResults()
    }

    public func itemMetadatas(
        account: String, underServerUrl serverUrl: String
    ) -> [SendableItemMetadata] {
        itemMetadatas
            .where { item in
                item.account == account &&
                    RealmItemMetadata.hasServerUrl(item, equalTo: serverUrl, includingDescendants: true)
            }
            .toUnmanagedResults()
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
        if let parent = parentDirectoryMetadataForItem(metadata) {
            return parent.keepDownloaded
        }

        if let root = itemMetadata(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue) {
            return root.keepDownloaded
        }

        return false
    }

    private func processItemMetadatasToDelete(
        existingMetadatas: Results<RealmItemMetadata>,
        updatedMetadatas: [SendableItemMetadata]
    ) -> [RealmItemMetadata] {
        var deletedMetadatas: [RealmItemMetadata] = []

        for existingMetadata in existingMetadatas {
            guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                  let metadataToDelete = itemMetadatas.where({ $0.ocId == existingMetadata.ocId }).first
            else { continue }

            deletedMetadatas.append(metadataToDelete)

            logger.debug("Deleting item metadata during update.", [.item: existingMetadata.ocId])
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(existingMetadatas: Results<RealmItemMetadata>, updatedMetadatas: [SendableItemMetadata], keepExistingDownloadState: Bool) -> (newMetadatas: [SendableItemMetadata], updatedMetadatas: [SendableItemMetadata], directoriesNeedingRename: [SendableItemMetadata]) {
        var returningNewMetadatas: [SendableItemMetadata] = []
        var returningUpdatedMetadatas: [SendableItemMetadata] = []
        var directoriesNeedingRename: [SendableItemMetadata] = []

        for var updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingMetadatas.first(where: { $0.ocId == updatedMetadata.ocId }) {
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
                updatedMetadata.keepDownloaded = inheritedKeepDownloaded(for: updatedMetadata)

                returningNewMetadatas.append(updatedMetadata)

                logger.debug("Created new item metadata during update.", [.item: updatedMetadata.ocId])
            }
        }

        return (returningNewMetadatas, returningUpdatedMetadatas, directoriesNeedingRename)
    }

    /// ONLY HANDLES UPDATES FOR IMMEDIATE CHILDREN
    /// (in case of directory renames/moves, the changes are recursed down)
    public func depth1ReadUpdateItemMetadatas(
        account: String,
        serverUrl: String,
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool
    ) -> ChangeSet? {
        let database = ncDatabase()

        do {
            // Find the metadatas that we previously knew to be on the server for this account
            // (we need to check if they were uploaded to prevent deleting ignored/lock files)
            //
            // - the ones that do exist remotely still are either the same or have been updated
            // - the ones that don't have been deleted
            var cleanServerUrl = serverUrl
            if cleanServerUrl.last == "/" {
                cleanServerUrl.removeLast()
            }
            let existingMetadatas = database
            .objects(RealmItemMetadata.self)
            .where { item in
                // Don't worry — root will be updated at the end of this method if is the target
                item.ocId != NSFileProviderItemIdentifier.rootContainer.rawValue &&
                    RealmItemMetadata.hasServerUrl(item, equalTo: cleanServerUrl, includingDescendants: false) &&
                    item.account == account &&
                    item.uploaded
                }

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
                var metadata = SendableItemMetadata(value: $0)
                metadata.deleted = true
                return metadata
            }

            let metadatasToChange = processItemMetadatasToUpdate(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedChildMetadatas,
                keepExistingDownloadState: keepExistingDownloadState
            )

            var metadatasToUpdate = metadatasToChange.updatedMetadatas
            var metadatasToCreate = metadatasToChange.newMetadatas
            let directoriesNeedingRename = metadatasToChange.directoriesNeedingRename

            for metadata in directoriesNeedingRename {
                if let updatedDirectoryChildren = renameDirectoryAndPropagateToChildren(
                    ocId: metadata.ocId,
                    newServerUrl: metadata.serverUrl,
                    newFileName: metadata.fileName
                ) {
                    metadatasToUpdate += updatedDirectoryChildren
                }
            }

            if var readTargetMetadata {
                if readTargetMetadata.directory {
                    readTargetMetadata.visitedDirectory = true
                }

                if let existing = itemMetadata(ocId: readTargetMetadata.ocId) {
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
                    readTargetMetadata.keepDownloaded = inheritedKeepDownloaded(for: readTargetMetadata)
                    metadatasToCreate.insert(readTargetMetadata, at: 0)
                }
            }

            try database.write {
                // Evict any logical-address duplicates before persisting fresh
                // payloads, so an ocId rotation (or rename whose target collides
                // with a third row) does not leave two non-deleted siblings at
                // the same `(account, serverUrl, fileName)`.
                for metadata in metadatasToCreate {
                    evictLogicalDuplicates(of: metadata, in: database)
                }
                for metadata in metadatasToUpdate {
                    evictLogicalDuplicates(of: metadata, in: database)
                }

                // Do not delete the metadatas that have been deleted
                database.add(metadatasToDelete.map { RealmItemMetadata(value: $0) }, update: .modified)
                database.add(metadatasToUpdate.map { RealmItemMetadata(value: $0) }, update: .modified)
                database.add(metadatasToCreate.map { RealmItemMetadata(value: $0) }, update: .all)
            }

            return ChangeSet(
                created: metadatasToCreate, updated: metadatasToUpdate, deleted: metadatasToDelete
            )
        } catch {
            logger.error("Could not update any item metadatas.", [.error: error])
            return nil
        }
    }

    /// If setting a downloading or uploading status, also modified the relevant boolean properties
    /// of the item metadata object
    public func setStatusForItemMetadata(
        _ metadata: SendableItemMetadata, status: Status
    ) -> SendableItemMetadata? {
        guard let result = itemMetadatas.where({ $0.ocId == metadata.ocId }).first else {
            logger.debug("Did not update status for item metadata as it was not found. ocID: \(metadata.ocId)")
            return nil
        }

        do {
            let database = ncDatabase()
            try database.write {
                result.status = status.rawValue
                if result.isDownload {
                    result.downloaded = false
                } else if result.isUpload {
                    result.uploaded = false
                }

                logger.debug("Updated status for item metadata.", [
                    .item: metadata.ocId,
                    .eTag: metadata.etag,
                    .name: metadata.fileName,
                    .syncTime: metadata.syncTime
                ])
            }
            return SendableItemMetadata(value: result)
        } catch {
            logger.error("Could not update status for item metadata.", [
                .item: metadata.ocId,
                .eTag: metadata.etag,
                .error: error,
                .name: metadata.fileName
            ])
        }

        return nil
    }

    public func addItemMetadata(_ metadata: SendableItemMetadata) {
        let database = ncDatabase()

        do {
            try database.write {
                evictLogicalDuplicates(of: metadata, in: database)
                database.add(RealmItemMetadata(value: metadata), update: .all)
                logger.debug("Added item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl])
            }
        } catch {
            logger.error("Failed to add item metadata.", [.item: metadata.ocId, .name: metadata.fileName, .url: metadata.serverUrl, .error: error])
        }
    }

    ///
    /// Add or replace `metadata` while carrying over local-only fields the
    /// server payload cannot know about: ``keepDownloaded``, ``downloaded``,
    /// ``visitedDirectory``, and ``lockToken``.
    ///
    /// Mirrors the preservation set applied by
    /// ``processItemMetadatasToUpdate`` for non-paginated reads. Use this from
    /// any code path that ingests fresh PROPFIND results (e.g. paginated
    /// enumeration); plain ``addItemMetadata(_:)`` would otherwise overwrite
    /// these fields back to their defaults via Realm's `update: .all`,
    /// silently undoing user-visible state such as "Always keep downloaded"
    /// (#9923).
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

        let metadatas = ncDatabase().objects(RealmItemMetadata.self)

        if let existing = metadatas.where({ $0.ocId == metadata.ocId }).first {
            toWrite.downloaded = existing.downloaded
            toWrite.keepDownloaded = existing.keepDownloaded

            if preserveVisitedDirectory {
                toWrite.visitedDirectory = existing.visitedDirectory
            }

            toWrite.lockToken = existing.lockToken
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
            // be evicted. Eviction in `addItemMetadata` will then prune the
            // prior row in the same write that persists the fresh one.
            let logicalCandidates = metadatas.where { item in
                RealmItemMetadata.hasLocation(
                    item,
                    account: metadata.account,
                    serverUrl: metadata.serverUrl,
                    fileName: metadata.fileName
                )
                    && !item.deleted
                    && !item.isLockFileOfLocalOrigin
            }

            if logicalCandidates.count == 1, let existing = logicalCandidates.first {
                toWrite.downloaded = existing.downloaded
                toWrite.keepDownloaded = existing.keepDownloaded

                if preserveVisitedDirectory {
                    toWrite.visitedDirectory = existing.visitedDirectory
                }

                toWrite.lockToken = existing.lockToken
            } else {
                // No prior row at this ocId or logical address: this is a
                // genuinely new item. Inherit the parent's "Always keep
                // downloaded" flag so a file surfacing here via remote
                // enumeration acquires the same pin as its already-pinned
                // siblings (#10054).
                toWrite.keepDownloaded = inheritedKeepDownloaded(for: metadata)
            }
        }

        addItemMetadata(toWrite)
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
        do {
            let results = itemMetadatas.where { $0.ocId == ocId }
            let database = ncDatabase()

            try database.write {
                results.forEach { $0.deleted = true }
                logger.debug("Marked item as deleted.", [.item: ocId])
            }

            return true
        } catch {
            logger.error("Could not mark item as deleted.", [.item: ocId, .error: error])
            return false
        }
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
        do {
            let database = ncDatabase()
            let results = itemMetadatas.where { $0.ocId == ocId }

            try database.write {
                database.delete(results)
                logger.debug("Removed item metadata from database.", [.item: ocId])
            }
        } catch {
            logger.error("Could not remove item metadata.", [.item: ocId, .error: error])
        }
    }

    public func renameItemMetadata(ocId: String, newServerUrl: String, newFileName: String) {
        guard let itemMetadata = itemMetadatas.where({ $0.ocId == ocId }).first else {
            logger.error("Could not find an item with ocID \(ocId) to rename to \(newFileName)")
            return
        }

        do {
            let database = ncDatabase()
            try database.write {
                let oldFileName = itemMetadata.fileName
                let oldServerUrl = itemMetadata.serverUrl

                itemMetadata.fileName = newFileName
                itemMetadata.fileNameView = newFileName
                itemMetadata.serverUrl = newServerUrl
                itemMetadata.normalizedServerUrl = newServerUrl.canonicalForm
                itemMetadata.normalizedFileName = newFileName.canonicalForm
                itemMetadata.lockToken = nil

                database.add(itemMetadata, update: .all)

                logger.debug("Renamed item \(oldFileName) to \(newFileName), moved from serverUrl: \(oldServerUrl) to serverUrl: \(newServerUrl)")
            }
        } catch {
            logger.error("Could not rename filename of item metadata with ocID: \(ocId) to proposed name \(newFileName) at proposed serverUrl \(newServerUrl).", [.error: error])
        }
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

    private func managedMaterialisedItemMetadatas() -> Results<RealmItemMetadata> {
        itemMetadatas.where { candidate in
            let isVisitedDirectory = candidate.directory && candidate.visitedDirectory
            let isDownloadedFile = candidate.directory == false && candidate.downloaded

            return isVisitedDirectory || isDownloadedFile
        }
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
        managedMaterialisedItemMetadatas().toUnmanagedResults()
    }

    ///
    /// Look up the not yet synchronized changes and deletions in the materialized items since the last given synchronization time.
    ///
    /// - Parameters:
    ///     - date: All items with a synchronization time later than this are considered.
    ///
    /// - Returns: Locally changed items in the working set grouped by "updated" and "deleted".
    ///
    public func pendingWorkingSetChanges(since date: Date) -> (updated: [SendableItemMetadata], deleted: [SendableItemMetadata]) {
        logger.debug("Gathering pending working set changes...")
        let pendingChanges = managedMaterialisedItemMetadatas().where { $0.syncTime > date }
        var updatedItems = pendingChanges.where { !$0.deleted }.toUnmanagedResults()
        var deletedItems = pendingChanges.where { $0.deleted }.toUnmanagedResults()

        for item in updatedItems {
            logger.debug("Found updated item.", [.item: item.ocId, .name: item.fileName])
        }

        for item in deletedItems {
            logger.debug("Found deleted item.", [.item: item.ocId, .name: item.fileName])
        }

        var updatedItemIdentifiers = Set(updatedItems.map(\.ocId))
        var deletedItemIdentifiers = Set(deletedItems.map(\.ocId))

        updatedItems // Look for changed children
            .filter {
                $0.directory // files do not have any children to look for
            }
            .map {
                $0.remotePath()
            }
            .forEach { serverUrl in
                itemMetadatas
                    .where { item in
                        RealmItemMetadata.hasServerUrl(item, equalTo: serverUrl, includingDescendants: false) &&
                            item.syncTime > date
                    }
                    .forEach { child in
                        let sendableMetadata = SendableItemMetadata(value: child)

                        if child.deleted {
                            guard deletedItemIdentifiers.contains(child.ocId) == false else {
                                return
                            }

                            deletedItemIdentifiers.insert(child.ocId)
                            deletedItems.append(sendableMetadata)
                            logger.debug("Appended deleted item to working set changes.", [.item: child.ocId, .url: serverUrl])
                        } else {
                            guard updatedItemIdentifiers.contains(child.ocId) == false else {
                                return
                            }

                            updatedItemIdentifiers.insert(child.ocId)
                            updatedItems.append(sendableMetadata)
                            logger.debug("Appended updated item to working set changes.", [.item: child.ocId, .url: serverUrl])
                        }
                    }
            }

        deletedItems // Look for deleted children recursively
            .filter {
                $0.directory // files do not have any children to look for
            }
            .map {
                $0.remotePath()
            }
            .forEach { serverUrl in
                itemMetadatas.where { item in
                    RealmItemMetadata.hasServerUrl(item, equalTo: serverUrl, includingDescendants: true) &&
                        item.syncTime > date
                }.forEach { child in
                    guard child.isLockFileOfLocalOrigin == false else {
                        logger.info("Excluding item from deletion because it is a lock file from local origin.", [.item: child.ocId, .name: child.fileName])
                        return
                    }

                    guard !deletedItemIdentifiers.contains(child.ocId) else {
                        return
                    }

                    deletedItemIdentifiers.insert(child.ocId)
                    deletedItems.append(SendableItemMetadata(value: child))
                    logger.debug("Appended deleted item to working set changes.", [.item: child.ocId, .url: serverUrl])
                }
            }

        return (updatedItems, deletedItems)
    }

    public func itemsMetadataByFileNameSuffix(suffix: String) -> [SendableItemMetadata] {
        logger.debug("Trying to find files matching pattern \"\(suffix)\".")

        let results = itemMetadatas.where {
            $0.fileName.ends(with: suffix) && !$0.directory
        }

        guard !results.isEmpty else {
            logger.debug("Could not find files matching pattern \"\(suffix)\".")
            return []
        }

        let filesMetadata = results.toUnmanagedResults()
        logger.debug("Found \(filesMetadata.count) file(s) that match \"\(suffix)\" metadata: \(filesMetadata)")

        return filesMetadata
    }
}
