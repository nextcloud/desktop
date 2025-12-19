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

    private static let schemaVersion = SchemaVersion.addedIsLockFileOfLocalOriginToRealmItemMetadata
    let logger: FileProviderLogger
    let account: Account

    var itemMetadatas: Results<RealmItemMetadata> { ncDatabase().objects(RealmItemMetadata.self) }

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

            },
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )

        Realm.Configuration.defaultConfiguration = configuration

        do {
            _ = try Realm()
            logger.info("Successfully created Realm.")
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
        guard let remoteURLComponents = URLComponents(string: rawRemoteURL) else {
            return nil
        }

        guard let remoteURL = remoteURLComponents.url else {
            return nil
        }

        // Get the file name but also take the possible fragment into consideration which is not part of a URL path but a file name.
        var fileName = remoteURL.lastPathComponent

        if let fragment = remoteURL.fragment {
            fileName = "\(fileName)#\(fragment)"
        }

        // Derive the parent address by removing the last path component and discarding the fragment which may actually be part of the file name and not a URL fragment.
        var parentURLComponents = remoteURLComponents
        parentURLComponents.path = remoteURL.deletingLastPathComponent().path
        parentURLComponents.fragment = nil

        guard var rawParentURL = parentURLComponents.url?.absoluteString.removingPercentEncoding else {
            return nil
        }

        // Remove any trailing slash.
        if rawParentURL.hasSuffix("/") {
            rawParentURL.removeLast()
        }

        if let metadata = itemMetadatas.where({
            $0.account == account && $0.serverUrl == rawParentURL && $0.fileName == fileName
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
            .where { $0.account == account && $0.serverUrl.starts(with: serverUrl) }
            .toUnmanagedResults()
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
                    if updatedMetadata.directory,
                       updatedMetadata.serverUrl != existingMetadata.serverUrl ||
                       updatedMetadata.fileName != existingMetadata.fileName
                    {
                        directoriesNeedingRename.append(updatedMetadata)
                    }

                    if keepExistingDownloadState {
                        updatedMetadata.downloaded = existingMetadata.downloaded
                    }

                    updatedMetadata.visitedDirectory = existingMetadata.visitedDirectory
                    updatedMetadata.keepDownloaded = existingMetadata.keepDownloaded
                    updatedMetadata.lockToken = existingMetadata.lockToken

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
                returningNewMetadatas.append(updatedMetadata)

                logger.debug("Created new item metadata during update.", [.item: updatedMetadata.ocId])
            }
        }

        return (returningNewMetadatas, returningUpdatedMetadatas, directoriesNeedingRename)
    }

    // ONLY HANDLES UPDATES FOR IMMEDIATE CHILDREN
    // (in case of directory renames/moves, the changes are recursed down)
    public func depth1ReadUpdateItemMetadatas(
        account: String,
        serverUrl: String,
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool
    ) -> (
        newMetadatas: [SendableItemMetadata]?,
        updatedMetadatas: [SendableItemMetadata]?,
        deletedMetadatas: [SendableItemMetadata]?
    ) {
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
                .where {
                    // Don't worry — root will be updated at the end of this method if is the target
                    $0.ocId != NSFileProviderItemIdentifier.rootContainer.rawValue &&
                        $0.account == account &&
                        $0.serverUrl == cleanServerUrl &&
                        $0.uploaded
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
                    metadatasToCreate.insert(readTargetMetadata, at: 0)
                }
            }

            try database.write {
                // Do not delete the metadatas that have been deleted
                database.add(metadatasToDelete.map { RealmItemMetadata(value: $0) }, update: .modified)
                database.add(metadatasToUpdate.map { RealmItemMetadata(value: $0) }, update: .modified)
                database.add(metadatasToCreate.map { RealmItemMetadata(value: $0) }, update: .all)
            }

            return (metadatasToCreate, metadatasToUpdate, metadatasToDelete)
        } catch {
            logger.error("Could not update any item metadatas.", [.error: error])
            return (nil, nil, nil)
        }
    }

    // If setting a downloading or uploading status, also modified the relevant boolean properties
    // of the item metadata object
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
                    result.chunkUploadId = UUID().uuidString
                } else if status == .normal, metadata.isUpload {
                    result.chunkUploadId = nil
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
                database.add(RealmItemMetadata(value: metadata), update: .all)
                logger.debug("Added item metadata.", [.item: metadata.ocId, .name: metadata.name, .url: metadata.serverUrl])
            }
        } catch {
            logger.error("Failed to add item metadata.", [.item: metadata.ocId, .name: metadata.name, .url: metadata.serverUrl, .error: error])
        }
    }

    @discardableResult public func deleteItemMetadata(ocId: String) -> Bool {
        do {
            let results = itemMetadatas.where { $0.ocId == ocId }
            let database = ncDatabase()
            try database.write {
                logger.debug("Deleting item metadata. \(ocId)")
                results.forEach { $0.deleted = true }
            }
            return true
        } catch {
            logger.error("Could not delete item metadata with ocId \(ocId).", [.error: error])
            return false
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

        let (metadatas, _, _, _, _, error) = await Enumerator.readServerUrl(
            metadata.serverUrl,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: self,
            depth: .target,
            log: logger.log
        )

        guard error == nil, let parentMetadata = metadatas?.first else {
            logger.error("Could not retrieve parent item identifier remotely.", [
                .error: error,
                .item: metadata.ocId,
                .name: metadata.fileName
            ])

            return nil
        }
        return NSFileProviderItemIdentifier(parentMetadata.ocId)
    }

    private func managedMaterialisedItemMetadatas(account: String) -> Results<RealmItemMetadata> {
        itemMetadatas.where { candidate in
            let belongsToAccount = candidate.account == account
            let isVisitedDirectory = candidate.directory && candidate.visitedDirectory
            let isDownloadedFile = candidate.directory == false && candidate.downloaded

            return belongsToAccount && (isVisitedDirectory || isDownloadedFile)
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
    public func materialisedItemMetadatas(account: String) -> [SendableItemMetadata] {
        managedMaterialisedItemMetadatas(account: account).toUnmanagedResults()
    }

    public func pendingWorkingSetChanges(account: Account, since date: Date) -> (updated: [SendableItemMetadata], deleted: [SendableItemMetadata]) {
        let accId = account.ncKitAccount
        let pending = managedMaterialisedItemMetadatas(account: accId).where { $0.syncTime > date }
        var updated = pending.where { !$0.deleted }.toUnmanagedResults()
        var deleted = pending.where { $0.deleted }.toUnmanagedResults()
        var handledUpdateOcIds = Set(updated.map(\.ocId))

        updated
            .map { $0.remotePath() }
            .forEach { serverUrl in
                logger.debug("Checking updated item...", [.url: serverUrl])

                itemMetadatas
                    .where { $0.serverUrl == serverUrl && $0.syncTime > date }
                    .forEach { metadata in
                        guard !handledUpdateOcIds.contains(metadata.ocId) else {
                            return
                        }

                        handledUpdateOcIds.insert(metadata.ocId)
                        let sendableMetadata = SendableItemMetadata(value: metadata)

                        if metadata.deleted {
                            deleted.append(sendableMetadata)
                            logger.debug("Appended deleted item to working set changes.", [.item: metadata.ocId, .url: serverUrl])
                        } else {
                            updated.append(sendableMetadata)
                            logger.debug("Appended updated item to working set changes.", [.item: metadata.ocId, .url: serverUrl])
                        }
                    }
            }

        let handledDeleteOcIds = Set(deleted.map(\.ocId))

        deleted
            .map { $0.remotePath() }
            .forEach { serverUrl in
                logger.debug("Verifying deleted item...", [.url: serverUrl])

                itemMetadatas.where {
                    $0.serverUrl.starts(with: serverUrl) && $0.syncTime > date
                }.forEach { metadata in
                    guard metadata.isLockFileOfLocalOrigin == false else {
                        logger.info("Excluding item from deletion because it is a lock file from local origin.", [.item: metadata.ocId])
                        return
                    }

                    guard !handledDeleteOcIds.contains(metadata.ocId) else {
                        return
                    }

                    deleted.append(SendableItemMetadata(value: metadata))
                    logger.debug("Appended deleted item to working set changes.", [.item: metadata.ocId, .url: serverUrl])
                }
            }

        return (updated, deleted)
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
