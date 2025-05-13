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

public let relativeDatabaseFolderPath = "Database/"
public let databaseFilename = "fileproviderextdatabase.realm"

public final class FilesDatabaseManager: Sendable {
    enum ErrorCode: Int {
        case metadataNotFound = -1000
    }

    private static let schemaVersion = stable2_0SchemaVersion
    static let errorDomain = "FilesDatabaseManager"
    static let logger = Logger(subsystem: Logger.subsystem, category: "filesdatabase")
    let account: Account

    var itemMetadatas: Results<RealmItemMetadata> { ncDatabase().objects(RealmItemMetadata.self) }

    public init(
        realmConfig: Realm.Configuration = Realm.Configuration.defaultConfiguration,
        account: Account,
        fileProviderDataDirUrl: URL? = pathForFileProviderExtData(),
        relativeDatabaseFolderPath: String = relativeDatabaseFolderPath
    ) {
        self.account = account

        let fm = FileManager.default
        let dbPath = realmConfig.fileURL?.path
        let migrate = dbPath != nil && !fm.fileExists(atPath: dbPath!)

        Realm.Configuration.defaultConfiguration = realmConfig

        do {
            _ = try Realm()
            Self.logger.info("Successfully started Realm db for NextcloudFileProviderKit")
        } catch let error {
            Self.logger.error("Error opening Realm db: \(error, privacy: .public)")
        }

        // Migrate from old unified database to new per-account DB
        guard let fileProviderDataDirUrl, migrate else { return }
        let oldRelativeDatabaseFilePath = relativeDatabaseFolderPath + databaseFilename
        let oldDatabasePath = fileProviderDataDirUrl.appendingPathComponent(
            oldRelativeDatabaseFilePath
        )
        guard FileManager.default.fileExists(atPath: oldDatabasePath.path) == true else {
            Self.logger.debug("No old database found at \(oldDatabasePath.path) skipping migration")
            return
        }
        Self.logger.info(
            "Migrating old database to database for \(account.ncKitAccount, privacy: .public)"
        )
        let oldConfig = Realm.Configuration(
            fileURL: oldDatabasePath,
            schemaVersion: stable2_0SchemaVersion,
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )
        do {
            let oldRealm = try Realm(configuration: oldConfig)
            let itemMetadatas = oldRealm
                .objects(RealmItemMetadata.self)
                .filter { $0.account == account.ncKitAccount }
            let remoteFileChunks = oldRealm.objects(RemoteFileChunk.self)
            Self.logger.info(
                "Migrating \(itemMetadatas.count) metadatas and \(remoteFileChunks.count) chunks"
            )

            let currentRealm = try Realm()
            try currentRealm.write {
                itemMetadatas.forEach { currentRealm.create(RealmItemMetadata.self, value: $0) }
                remoteFileChunks.forEach { currentRealm.create(RemoteFileChunk.self, value: $0) }
            }
        } catch let error {
            Self.logger.error(
                """
                Error migrating old database to account-specific database
                    for: \(account.ncKitAccount, privacy: .public)
                    Received error: \(error, privacy: .public)
                """
            )
        }
    }

    public convenience init?(account: Account) {
        let relativeDatabaseFilePath =
            relativeDatabaseFolderPath + account.fileName + "-" + databaseFilename
        guard let fileProviderDataDirUrl = pathForFileProviderExtData() else { return nil }
        let databasePath = fileProviderDataDirUrl.appendingPathComponent(relativeDatabaseFilePath)

        // Disable file protection for directory DB
        // https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/
        let dbFolder = fileProviderDataDirUrl.appendingPathComponent(relativeDatabaseFolderPath)
        let dbFolderPath = dbFolder.path
        do {
            try FileManager.default.createDirectory(at: dbFolder, withIntermediateDirectories: true)
            try FileManager.default.setAttributes(
                [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication],
                ofItemAtPath: dbFolderPath
            )
        } catch {
            Self.logger.error(
                "Could not set permission level for db folder: \(error, privacy: .public)"
            )
        }

        let config = Realm.Configuration(
            fileURL: databasePath,
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
        self.init(realmConfig: config, account: account)
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
                  let metadataToDelete = itemMetadatas.where({ $0.ocId == existingMetadata.ocId }).first
            else { continue }

            deletedMetadatas.append(metadataToDelete)

            Self.logger.debug(
                "Deleting item metadata during update. ocID: \(existingMetadata.ocId, privacy: .public), etag: \(existingMetadata.etag, privacy: .public), fileName: \(existingMetadata.fileName, privacy: .public)"
            )
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(
        existingMetadatas: Results<RealmItemMetadata>,
        updatedMetadatas: [SendableItemMetadata],
        updateDirectoryEtags: Bool,
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
                    if updatedMetadata.directory {
                        if updatedMetadata.serverUrl != existingMetadata.serverUrl
                            || updatedMetadata.fileName != existingMetadata.fileName
                        {
                            directoriesNeedingRename.append(updatedMetadata)
                            updatedMetadata.etag = ""  // Renaming doesn't change the etag so reset

                        } else if !updateDirectoryEtags {
                            updatedMetadata.etag = existingMetadata.etag
                        }
                    }

                    if keepExistingDownloadState {
                        updatedMetadata.downloaded = existingMetadata.downloaded
                    }

                    returningUpdatedMetadatas.append(updatedMetadata)

                    Self.logger.debug(
                        """
                        Updated existing item metadata.
                            ocID: \(updatedMetadata.ocId, privacy: .public)
                            etag: \(updatedMetadata.etag, privacy: .public)
                            fileName: \(updatedMetadata.fileName, privacy: .public)
                        """
                    )
                } else {
                    Self.logger.debug(
                        """
                        Skipping item metadata update; same as existing, or still in transit.
                            ocID: \(updatedMetadata.ocId, privacy: .public)
                            etag: \(updatedMetadata.etag, privacy: .public)
                            fileName: \(updatedMetadata.fileName, privacy: .public)
                        """
                    )
                }

            } else {  // This is a new metadata
                if !updateDirectoryEtags, updatedMetadata.directory {
                    updatedMetadata.etag = ""
                }

                returningNewMetadatas.append(updatedMetadata)

                Self.logger.debug(
                    """
                    Created new item metadata during update.
                        ocID: \(updatedMetadata.ocId, privacy: .public)
                        etag: \(updatedMetadata.etag, privacy: .public)
                        fileName: \(updatedMetadata.fileName, privacy: .public)
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
        updateDirectoryEtags: Bool,
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
            let existingMetadatas = database
                .objects(RealmItemMetadata.self)
                .where { $0.account == account && $0.serverUrl == serverUrl && $0.uploaded }

            // NOTE: These metadatas are managed -- be careful!
            let metadatasToDelete = processItemMetadatasToDelete(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedMetadatas)
            let metadatasToDeleteCopy = metadatasToDelete.map { SendableItemMetadata(value: $0) }

            let metadatasToChange = processItemMetadatasToUpdate(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedMetadatas,
                updateDirectoryEtags: updateDirectoryEtags,
                keepExistingDownloadState: keepExistingDownloadState
            )

            var metadatasToUpdate = metadatasToChange.updatedMetadatas
            let metadatasToCreate = metadatasToChange.newMetadatas
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

            try database.write {
                database.delete(metadatasToDelete)
                database.add(metadatasToUpdate.map { RealmItemMetadata(value: $0) }, update: .modified)
                database.add(metadatasToCreate.map { RealmItemMetadata(value: $0) }, update: .all)
            }

            return (metadatasToCreate, metadatasToUpdate, metadatasToDeleteCopy)
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
                database.delete(results)
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

        guard let itemParentDirectory = parentDirectoryMetadataForItem(metadata) else {
            Self.logger.error(
                """
                Could not get item parent directory metadata for metadata.
                    ocID: \(metadata.ocId, privacy: .public),
                    etag: \(metadata.etag, privacy: .public),
                    fileName: \(metadata.fileName, privacy: .public),
                    serverUrl: \(metadata.serverUrl, privacy: .public),
                    account: \(metadata.account, privacy: .public),
                """
            )
            return nil
        }

        if let parentDirectoryMetadata = itemMetadata(ocId: itemParentDirectory.ocId) {
            return NSFileProviderItemIdentifier(parentDirectoryMetadata.ocId)
        }

        Self.logger.error(
            """
            Could not get item parent directory item metadata for metadata.
                ocID: \(metadata.ocId, privacy: .public),
                etag: \(metadata.etag, privacy: .public), 
                fileName: \(metadata.fileName, privacy: .public),
                serverUrl: \(metadata.serverUrl, privacy: .public),
                account: \(metadata.account, privacy: .public),
            """
        )
        return nil
    }
}
