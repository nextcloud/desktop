/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
import Foundation
import OSLog
import RealmSwift

internal let stable1_0SchemaVersion: UInt64 = 100
internal let stable2_0SchemaVersion: UInt64 = 200 // Major change: deleted LocalFileMetadata type

///
/// Realm database abstraction and management.
///
public final class FilesDatabaseManager: Sendable {
    ///
    /// File name suffix for Realm database files.
    ///
    /// In the past, before account-specific databases, there was a single and shared database which had this file name.
    ///
    /// > Important: The value must not change, as it is used to migrate from the old unified database to the new per-account databases.
    ///
    static let databaseFilename = "fileproviderextdatabase.realm"

    public enum ErrorCode: Int {
        case metadataNotFound = -1000
        case parentMetadataNotFound = -1001
    }

    public enum ErrorUserInfoKey: String {
        case missingParentServerUrlAndFileName = "MissingParentServerUrlAndFileName"
    }
    
    static let errorDomain = "FilesDatabaseManager"

    static func error(code: ErrorCode, userInfo: [String: String]) -> NSError {
        NSError(domain: Self.errorDomain, code: code.rawValue, userInfo: userInfo)
    }

    static func parentMetadataNotFoundError(itemUrl: String) -> NSError {
        error(
            code: .parentMetadataNotFound,
            userInfo: [ErrorUserInfoKey.missingParentServerUrlAndFileName.rawValue: itemUrl]
        )
    }

    private static let schemaVersion = stable2_0SchemaVersion
    static let logger = Logger(subsystem: Logger.subsystem, category: "FilesDatabaseManager")
    let account: Account

    var itemMetadatas: Results<RealmItemMetadata> { ncDatabase().objects(RealmItemMetadata.self) }

    ///
    /// Check for the existence of the directory where to place database files and return it.
    ///
    /// - Returns: The location of the database files directory.
    ///
    private static func assertDatabaseDirectory() -> URL {
        Self.logger.debug("Asserting existence of database directory...")

        let manager = FileManager.default

        guard let fileProviderExtensionDataDirectory = urlForFileProviderExtensionData() else {
            Self.logger.fault("Failed to resolve the file provider extension data directory!")
            assertionFailure("Failed to resolve the file provider extension data directory!")
            return manager.temporaryDirectory // Only to satisfy the non-optional return type. The extension is unusable at this point anyway.
        }

        let databaseDirectory = fileProviderExtensionDataDirectory.appendingPathComponent("Database", isDirectory: true)
        let exists = manager.fileExists(atPath: databaseDirectory.path)

        if exists {
            Self.logger.info("Database directory exists at: \(databaseDirectory.path, privacy: .public)")
        } else {
            Self.logger.info("Due to nonexistent \"Database\" directory, assume it is not a legacy location and returning file provider extension data directory at: \(fileProviderExtensionDataDirectory.path, privacy: .public)")
            return fileProviderExtensionDataDirectory
        }

        // Disable file protection for database directory.
        // See: https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/

        do {
            try FileManager.default.setAttributes([.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: databaseDirectory.path)
            Self.logger.info("Set protectionKey attribute for database directory to FileProtectionType.completeUntilFirstUserAuthentication.")
        } catch {
            Self.logger.error("Could not set protectionKey attribute to FileProtectionType.completeUntilFirstUserAuthentication for database directory: \(error, privacy: .public)")
        }

        return databaseDirectory
    }

    ///
    /// Convenience initializer which defines a default configuration for Realm.
    ///
    /// - Parameters:
    ///     - customConfiguration: Optional custom Realm configuration to use instead of the default one.
    ///     - account: The Nextcloud account for which the database is being created.
    ///     - customDatabaseDirectory: Optional custom directory where the database files should be stored. If not provided, the default directory will be used.
    ///
    public init(realmConfiguration customConfiguration: Realm.Configuration? = nil, account: Account, databaseDirectory customDatabaseDirectory: URL? = nil) {
        self.account = account

        Self.logger.info("Initializing for account: \(account.ncKitAccount, privacy: .public)")

        let databaseDirectory = customDatabaseDirectory ?? Self.assertDatabaseDirectory()
        let accountDatabaseFilename = account.fileName + "-" + Self.databaseFilename
        let databaseLocation = databaseDirectory.appendingPathComponent(accountDatabaseFilename)

        let configuration = customConfiguration ?? Realm.Configuration(
            fileURL: databaseLocation,
            schemaVersion: Self.schemaVersion,
            migrationBlock: { migration, oldSchemaVersion in
                if oldSchemaVersion == stable1_0SchemaVersion {
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

        let fileManager = FileManager.default
        let databasePathFromRealmConfiguration = configuration.fileURL?.path
        let migrate = databasePathFromRealmConfiguration != nil && fileManager.fileExists(atPath: databasePathFromRealmConfiguration!) == false

        do {
            _ = try Realm()
            Self.logger.info("Successfully created Realm.")
        } catch let error {
            Self.logger.fault("Error creating Realm: \(error, privacy: .public)")
        }

        // Migrate from old unified database to new per-account DB
        guard migrate else {
            Self.logger.debug("No migration needed for \(account.ncKitAccount, privacy: .public)")
            return
        }

        let sharedDatabaseURL = databaseDirectory.appendingPathComponent(Self.databaseFilename)

        guard FileManager.default.fileExists(atPath: sharedDatabaseURL.path) == true else {
            Self.logger.debug("No shared legacy database found at \"\(sharedDatabaseURL.path)\", skipping migration.")
            return
        }

        Self.logger.info("Migrating shared legacy database to new database for \(account.ncKitAccount, privacy: .public)")

        let legacyConfiguration = Realm.Configuration(fileURL: sharedDatabaseURL, schemaVersion: stable2_0SchemaVersion, objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self])

        do {
            let legacyRealm = try Realm(configuration: legacyConfiguration)

            let itemMetadatas = legacyRealm
                .objects(RealmItemMetadata.self)
                .filter { $0.account == account.ncKitAccount }

            let remoteFileChunks = legacyRealm.objects(RemoteFileChunk.self)

            Self.logger.info("Migrating \(itemMetadatas.count) metadatas and \(remoteFileChunks.count) chunks.")

            let currentRealm = try Realm()

            try currentRealm.write {
                itemMetadatas.forEach { currentRealm.create(RealmItemMetadata.self, value: $0) }
                remoteFileChunks.forEach { currentRealm.create(RemoteFileChunk.self, value: $0) }
            }
        } catch let error {
            Self.logger.error("Error migrating shared legacy database to account-specific database for: \(account.ncKitAccount, privacy: .public) because of error: \(error, privacy: .public)")
        }
    }

    func ncDatabase() -> Realm {
        let realm = try! Realm()
        realm.refresh()
        return realm
    }

    public func anyItemMetadatasForAccount(_ account: String) -> Bool {
        !itemMetadatas.where({ $0.account == account }).isEmpty
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

    public func itemMetadata(
        account: String, locatedAtRemoteUrl remoteUrl: String // Is the URL for the actual item
    ) -> SendableItemMetadata? {
        guard let actualRemoteUrl = URL(string: remoteUrl) else { return nil }
        let fileName = actualRemoteUrl.lastPathComponent
        guard var serverUrl = actualRemoteUrl
            .deletingLastPathComponent()
            .absoluteString
            .removingPercentEncoding
        else { return nil }
        if serverUrl.hasSuffix("/") {
            serverUrl.removeLast()
        }
        if let metadata = itemMetadatas.where({
            $0.account == account && $0.serverUrl == serverUrl && $0.fileName == fileName
        }).first {
            return SendableItemMetadata(value: metadata)
        }
        return nil
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

    public func itemMetadataFromFileProviderItemIdentifier(
        _ identifier: NSFileProviderItemIdentifier
    ) -> SendableItemMetadata? {
        itemMetadata(ocId: identifier.rawValue)
    }

    private func processItemMetadatasToDelete(
        existingMetadatas: Results<RealmItemMetadata>,
        updatedMetadatas: [SendableItemMetadata]
    ) -> [RealmItemMetadata] {
        var deletedMetadatas: [RealmItemMetadata] = []

        for existingMetadata in existingMetadatas {
            guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                  var metadataToDelete = itemMetadatas.where({ $0.ocId == existingMetadata.ocId }).first
            else { continue }

            deletedMetadatas.append(metadataToDelete)

            Self.logger.debug(
                """
                Deleting item metadata during update.
                    ocID: \(existingMetadata.ocId, privacy: .public)
                    etag: \(existingMetadata.etag, privacy: .public)
                    fileName: \(existingMetadata.fileName, privacy: .public)"
                    syncTime: \(existingMetadata.syncTime, privacy: .public)
                """
            )
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(
        existingMetadatas: Results<RealmItemMetadata>,
        updatedMetadatas: [SendableItemMetadata],
        keepExistingDownloadState: Bool
    ) -> (
        newMetadatas: [SendableItemMetadata],
        updatedMetadatas: [SendableItemMetadata],
        directoriesNeedingRename: [SendableItemMetadata]
    ) {
        var returningNewMetadatas: [SendableItemMetadata] = []
        var returningUpdatedMetadatas: [SendableItemMetadata] = []
        var directoriesNeedingRename: [SendableItemMetadata] = []

        for var updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingMetadatas.first(where: {
                $0.ocId == updatedMetadata.ocId
            }) {
                if existingMetadata.status == Status.normal.rawValue,
                    !existingMetadata.isInSameDatabaseStoreableRemoteState(updatedMetadata)
                {
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

                    returningUpdatedMetadatas.append(updatedMetadata)

                    Self.logger.debug(
                        """
                        Updated existing item metadata.
                            ocID: \(updatedMetadata.ocId, privacy: .public)
                            etag: \(updatedMetadata.etag, privacy: .public)
                            fileName: \(updatedMetadata.fileName, privacy: .public)
                            syncTime: \(updatedMetadata.syncTime, privacy: .public)
                        """
                    )
                } else {
                    Self.logger.debug(
                        """
                        Skipping item metadata update; same as existing, or still in transit.
                            ocID: \(updatedMetadata.ocId, privacy: .public)
                            etag: \(updatedMetadata.etag, privacy: .public)
                            fileName: \(updatedMetadata.fileName, privacy: .public)
                            syncTime: \(updatedMetadata.syncTime, privacy: .public)
                        """
                    )
                }

            } else { // This is a new metadata
                returningNewMetadatas.append(updatedMetadata)

                Self.logger.debug(
                    """
                    Created new item metadata during update.
                        ocID: \(updatedMetadata.ocId, privacy: .public)
                        etag: \(updatedMetadata.etag, privacy: .public)
                        fileName: \(updatedMetadata.fileName, privacy: .public)
                        parentDirectoryUrl: \(updatedMetadata.serverUrl, privacy: .public)
                        account: \(updatedMetadata.account, privacy: .public)
                        content type: \(updatedMetadata.contentType, privacy: .public)
                        is directory: \(updatedMetadata.directory, privacy: .public)
                        creation date: \(updatedMetadata.creationDate, privacy: .public)
                        date: \(updatedMetadata.date, privacy: .public)
                        lock: \(updatedMetadata.lock, privacy: .public)
                        lockTimeOut: \(updatedMetadata.lockTimeOut?.description ?? "", privacy: .public)
                        lockOwner: \(updatedMetadata.lockOwner ?? "", privacy: .public)
                        permissions: \(updatedMetadata.permissions, privacy: .public)
                        size: \(updatedMetadata.size, privacy: .public)
                        trashbinFileName: \(updatedMetadata.trashbinFileName, privacy: .public)
                        downloaded: \(updatedMetadata.downloaded, privacy: .public)
                        uploaded: \(updatedMetadata.uploaded, privacy: .public)
                        visitedDirectory: \(updatedMetadata.visitedDirectory, privacy: .public)
                        keepDownloaded: \(updatedMetadata.keepDownloaded, privacy: .public)
                        deleted: \(updatedMetadata.deleted, privacy: .public)
                        syncTime: \(updatedMetadata.syncTime, privacy: .public)
                    """
                )
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

            let readTargetMetadata: SendableItemMetadata?
            if let targetMetadata = updatedMetadatas.first {
                if targetMetadata.directory {
                    readTargetMetadata = updatedChildMetadatas.removeFirst()
                } else {
                    readTargetMetadata = targetMetadata
                }
            } else {
                readTargetMetadata = nil
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
                    newFileName: metadata.fileName)
                {
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
                        Self.logger.info("Depth 1 read target changed: \(readTargetMetadata.ocId, privacy: .public)")
                        if keepExistingDownloadState {
                            readTargetMetadata.downloaded = existing.downloaded
                        }
                        readTargetMetadata.keepDownloaded = existing.keepDownloaded
                        metadatasToUpdate.insert(readTargetMetadata, at: 0)
                    }
                } else {
                    Self.logger.info("Depth 1 read target is new: \(readTargetMetadata.ocId, privacy: .public)")
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
            Self.logger.error(
                """
                Could not update any item metadatas.
                    Received error: \(error.localizedDescription, privacy: .public)
                """
            )
            return (nil, nil, nil)
        }
    }

    // If setting a downloading or uploading status, also modified the relevant boolean properties
    // of the item metadata object
    public func setStatusForItemMetadata(
        _ metadata: SendableItemMetadata, status: Status
    ) -> SendableItemMetadata? {
        guard let result = itemMetadatas.where({ $0.ocId == metadata.ocId }).first else {
            Self.logger.debug(
                """
                Did not update status for item metadata as it was not found.
                    ocID: \(metadata.ocId, privacy: .public)
                """
            )
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

                Self.logger.debug(
                    """
                    Updated status for item metadata.
                        ocID: \(metadata.ocId, privacy: .public)
                        etag: \(metadata.etag, privacy: .public)
                        fileName: \(metadata.fileName, privacy: .public)
                        syncTime: \(metadata.syncTime, privacy: .public)
                    """
                )
            }
            return SendableItemMetadata(value: result)
        } catch {
            Self.logger.error(
                """
                Could not update status for item metadata.
                    ocID: \(metadata.ocId, privacy: .public)
                    etag: \(metadata.etag, privacy: .public)
                    fileName: \(metadata.fileName, privacy: .public)
                    received error: \(error.localizedDescription, privacy: .public)
                """
            )
        }
        
        return nil
    }

    public func addItemMetadata(_ metadata: SendableItemMetadata) {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(RealmItemMetadata(value: metadata), update: .all)
                Self.logger.debug(
                    """
                    Added item metadata.
                        ocID: \(metadata.ocId, privacy: .public)
                        etag: \(metadata.etag, privacy: .public)
                        fileName: \(metadata.fileName, privacy: .public)
                        parentDirectoryUrl: \(metadata.serverUrl, privacy: .public)
                        account: \(metadata.account, privacy: .public)
                        content type: \(metadata.contentType, privacy: .public)
                        is directory: \(metadata.directory, privacy: .public)
                        creation date: \(metadata.creationDate, privacy: .public)
                        date: \(metadata.date, privacy: .public)
                        lock: \(metadata.lock, privacy: .public)
                        lockTimeOut: \(metadata.lockTimeOut?.description ?? "", privacy: .public)
                        lockOwner: \(metadata.lockOwner ?? "", privacy: .public)
                        permissions: \(metadata.permissions, privacy: .public)
                        size: \(metadata.size, privacy: .public)
                        trashbinFileName: \(metadata.trashbinFileName, privacy: .public)
                        downloaded: \(metadata.downloaded, privacy: .public)
                        uploaded: \(metadata.uploaded, privacy: .public)
                        visitedDirectory: \(metadata.visitedDirectory, privacy: .public)
                        keepDownloaded: \(metadata.keepDownloaded, privacy: .public)
                        deleted: \(metadata.deleted, privacy: .public)
                        syncTime: \(metadata.syncTime, privacy: .public)
                    """
                )
            }
        } catch {
            Self.logger.error(
                """
                Could not add item metadata.
                    ocID: \(metadata.ocId, privacy: .public)
                    etag: \(metadata.etag, privacy: .public)
                    fileName: \(metadata.fileName, privacy: .public)
                    syncTime: \(metadata.syncTime, privacy: .public)
                    received error: \(error.localizedDescription, privacy: .public)
                """
            )
        }
    }

    @discardableResult public func deleteItemMetadata(ocId: String) -> Bool {
        do {
            let results = itemMetadatas.where { $0.ocId == ocId }
            let database = ncDatabase()
            try database.write {
                Self.logger.debug("Deleting item metadata. \(ocId, privacy: .public)")
                results.forEach { $0.deleted = true }
            }
            return true
        } catch {
            Self.logger.error(
                """
                Could not delete item metadata with ocId: \(ocId, privacy: .public)
                    Received error: \(error.localizedDescription, privacy: .public)
                """
            )
            return false
        }
    }

    public func renameItemMetadata(ocId: String, newServerUrl: String, newFileName: String) {
        guard let itemMetadata = itemMetadatas.where({ $0.ocId == ocId }).first else {
            Self.logger.debug(
                """
                Could not find an item with ocID \(ocId, privacy: .public)
                    to rename to \(newFileName, privacy: .public)
                """
            )
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

                Self.logger.debug(
                    """
                    Renamed item \(oldFileName, privacy: .public) 
                    to \(newFileName, privacy: .public),
                    moved from serverUrl: \(oldServerUrl, privacy: .public)
                    to serverUrl: \(newServerUrl, privacy: .public)
                    """
                )
            }
        } catch {
            Self.logger.error(
                """
                Could not rename filename of item metadata with ocID: \(ocId, privacy: .public)
                    to proposed name \(newFileName, privacy: .public)
                    at proposed serverUrl \(newServerUrl, privacy: .public)
                    received error: \(error.localizedDescription, privacy: .public)
                """
            )
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
            Self.logger.error(
                """
                Could not get item parent directory item metadata for metadata.
                    ocID: \(metadata.ocId, privacy: .public),
                    etag: \(metadata.etag, privacy: .public), 
                    fileName: \(metadata.fileName, privacy: .public),
                    serverUrl: \(metadata.serverUrl, privacy: .public),
                    account: \(metadata.account, privacy: .public),
                    syncTime: \(metadata.syncTime, privacy: .public)
                """
            )
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
            depth: .target
        )
        guard error == nil, let parentMetadata = metadatas?.first else {
            Self.logger.error(
                """
                Could not retrieve parent item identifier remotely, received error.
                    target metadata: \(metadata.ocId, privacy: .public)
                    target filename: \(metadata.fileName, privacy: .public)
                    received metadatas: \(metadatas?.count ?? 0, privacy: .public)
                    error: \(error?.errorDescription ?? "NO ERROR", privacy: .public)
                """
            )
            return nil
        }
        return NSFileProviderItemIdentifier(parentMetadata.ocId)
    }

    private func managedMaterialisedItemMetadatas(account: String) -> Results<RealmItemMetadata> {
        itemMetadatas
            .where {
                $0.account == account &&
                (($0.directory && $0.visitedDirectory) || (!$0.directory && $0.downloaded))
            }
    }

    public func materialisedItemMetadatas(account: String) -> [SendableItemMetadata] {
        managedMaterialisedItemMetadatas(account: account).toUnmanagedResults()
    }

    public func pendingWorkingSetChanges(
        account: Account, since date: Date
    ) -> (updated: [SendableItemMetadata], deleted: [SendableItemMetadata]) {
        let accId = account.ncKitAccount
        let pending = managedMaterialisedItemMetadatas(account: accId).where { $0.syncTime > date }
        var updated = pending.where { !$0.deleted }.toUnmanagedResults()
        var deleted = pending.where { $0.deleted }.toUnmanagedResults()

        var handledUpdateOcIds = Set(updated.map(\.ocId))
        updated
            .map {
                var serverUrl = $0.serverUrl + "/" + $0.fileName
                if serverUrl.last == "/" { serverUrl.removeLast() }
                return serverUrl
            }
            .forEach { serverUrl in
                Self.logger.debug("Checking (updated) \(serverUrl, privacy: .public)")
                itemMetadatas
                    .where { $0.serverUrl == serverUrl && $0.syncTime > date }
                    .forEach { metadata in
                        Self.logger.debug("Checking item: \(metadata.fileName, privacy: .public)")
                        guard !handledUpdateOcIds.contains(metadata.ocId) else { return }
                        handledUpdateOcIds.insert(metadata.ocId)
                        let sendableMetadata = SendableItemMetadata(value: metadata)
                        if metadata.deleted {
                            deleted.append(sendableMetadata)
                        } else {
                            updated.append(sendableMetadata)
                        }
                        Self.logger.debug("Appended item: \(metadata.fileName, privacy: .public)")
                    }
            }

        var handledDeleteOcIds = Set(deleted.map(\.ocId))
        deleted
            .map {
                var serverUrl = $0.serverUrl + "/" + $0.fileName
                if serverUrl.last == "/" { serverUrl.removeLast() }
                return serverUrl
            }
            .forEach { serverUrl in
                Self.logger.debug("Checking (deletion) \(serverUrl, privacy: .public)")
                itemMetadatas
                    .where { $0.serverUrl.starts(with: serverUrl) && $0.syncTime > date }
                    .forEach { metadata in
                        guard !handledDeleteOcIds.contains(metadata.ocId) else { return }
                        deleted.append(SendableItemMetadata(value: metadata))
                    }
            }

        return (updated, deleted)
    }
}
