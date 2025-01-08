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

fileprivate let stable1_0SchemaVersion: UInt64 = 100
fileprivate let stable2_0SchemaVersion: UInt64 = 200 // Major change: deleted LocalFileMetadata type

public class FilesDatabaseManager {
    public static let shared = FilesDatabaseManager()!

    private static let relativeDatabaseFolderPath = "Database/"
    private static let databaseFilename = "fileproviderextdatabase.realm"
    private static let schemaVersion = stable2_0SchemaVersion

    static let logger = Logger(subsystem: Logger.subsystem, category: "filesdatabase")

    var itemMetadatas: Results<ItemMetadata> { ncDatabase().objects(ItemMetadata.self) }

    public init(realmConfig: Realm.Configuration = Realm.Configuration.defaultConfiguration) {
        Realm.Configuration.defaultConfiguration = realmConfig

        do {
            _ = try Realm()
            Self.logger.info("Successfully started Realm db for NextcloudFileProviderKit")
        } catch let error {
            Self.logger.error("Error opening Realm db: \(error, privacy: .public)")
        }
    }

    public convenience init?() {
        let relativeDatabaseFilePath = Self.relativeDatabaseFolderPath + Self.databaseFilename
        guard let fileProviderDataDirUrl = pathForFileProviderExtData() else { return nil }
        let databasePath = fileProviderDataDirUrl.appendingPathComponent(relativeDatabaseFilePath)

        // Disable file protection for directory DB
        // https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/
        let dbFolder = fileProviderDataDirUrl.appendingPathComponent(
            Self.relativeDatabaseFolderPath
        )
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

                    migration.enumerateObjects(ofType: ItemMetadata.className()) { _, newObject in
                        guard let newObject,
                              let imOcId = newObject["ocId"] as? String,
                              localFileMetadataOcIds.contains(imOcId)
                        else { return }
                        newObject["downloaded"] = true
                        newObject["uploaded"] = true
                    }
                }

            },
            objectTypes: [ItemMetadata.self, RemoteFileChunk.self]
        )
        self.init(realmConfig: config)
    }

    func ncDatabase() -> Realm {
        let realm = try! Realm()
        if !realm.refresh() {
            Self.logger.error("Failed to refresh Realm!!")
        }
        return realm
    }

    public func anyItemMetadatasForAccount(_ account: String) -> Bool {
        !itemMetadatas.filter("account == %@", account).isEmpty
    }

    public func itemMetadata(ocId: String, managed: Bool = false) -> ItemMetadata? {
        // Realm objects are live-fire, i.e. they will be changed and invalidated according to
        // changes in the db.
        //
        // Let's therefore create a copy
        if let itemMetadata = itemMetadatas.filter("ocId == %@", ocId).first {
            return managed ? itemMetadata : ItemMetadata(value: itemMetadata)
        }

        return nil
    }

    public func itemMetadata(
        account: String, locatedAtRemoteUrl remoteUrl: String // Is the URL for the actual item
    ) -> ItemMetadata? {
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
        if let metadata = itemMetadatas.filter(
            "account == %@ AND serverUrl == %@ AND fileName == %@", account, serverUrl, fileName
        ).first {
            return ItemMetadata(value: metadata)
        }
        return nil
    }

    public func itemMetadatas(account: String) -> [ItemMetadata] {
        itemMetadatas.filter("account == %@", account).toUnmanagedResults()
    }

    public func itemMetadatas(account: String, underServerUrl serverUrl: String) -> [ItemMetadata] {
        itemMetadatas
            .filter("account == %@ AND serverUrl BEGINSWITH %@", account, serverUrl)
            .toUnmanagedResults()
    }

    public func itemMetadataFromFileProviderItemIdentifier(
        _ identifier: NSFileProviderItemIdentifier
    ) -> ItemMetadata? {
        itemMetadata(ocId: identifier.rawValue)
    }

    private func processItemMetadatasToDelete(
        existingMetadatas: Results<ItemMetadata>,
        updatedMetadatas: [ItemMetadata]
    ) -> [ItemMetadata] {
        var deletedMetadatas: [ItemMetadata] = []

        for existingMetadata in existingMetadatas {
            guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                  let metadataToDelete = itemMetadata(ocId: existingMetadata.ocId, managed: true)
            else { continue }

            deletedMetadatas.append(metadataToDelete)

            Self.logger.debug(
                "Deleting item metadata during update. ocID: \(existingMetadata.ocId, privacy: .public), etag: \(existingMetadata.etag, privacy: .public), fileName: \(existingMetadata.fileName, privacy: .public)"
            )
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(
        existingMetadatas: Results<ItemMetadata>,
        updatedMetadatas: [ItemMetadata],
        updateDirectoryEtags: Bool
    ) -> (
        newMetadatas: [ItemMetadata], updatedMetadatas: [ItemMetadata],
        directoriesNeedingRename: [ItemMetadata]
    ) {
        var returningNewMetadatas: [ItemMetadata] = []
        var returningUpdatedMetadatas: [ItemMetadata] = []
        var directoriesNeedingRename: [ItemMetadata] = []

        for updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingMetadatas.first(where: {
                $0.ocId == updatedMetadata.ocId
            }) {
                if existingMetadata.status == ItemMetadata.Status.normal.rawValue,
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

    public func updateItemMetadatas(
        account: String,
        serverUrl: String,
        updatedMetadatas: [ItemMetadata],
        updateDirectoryEtags: Bool
    ) -> (
        newMetadatas: [ItemMetadata]?,
        updatedMetadatas: [ItemMetadata]?,
        deletedMetadatas: [ItemMetadata]?
    ) {
        let database = ncDatabase()

        do {
            let existingMetadatas = database.objects(ItemMetadata.self).filter(
                "account == %@ AND serverUrl == %@ AND status == %@", 
                account,
                serverUrl,
                ItemMetadata.Status.normal.rawValue)

            // NOTE: These metadatas are managed -- be careful!
            let metadatasToDelete = processItemMetadatasToDelete(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedMetadatas)
            let metadatasToDeleteCopy = metadatasToDelete.map { ItemMetadata(value: $0) }

            let metadatasToChange = processItemMetadatasToUpdate(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedMetadatas,
                updateDirectoryEtags: updateDirectoryEtags)

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
                database.add(metadatasToUpdate.map { ItemMetadata(value: $0) }, update: .modified)
                database.add(metadatasToCreate.map { ItemMetadata(value: $0) }, update: .all)
            }

            return (
                newMetadatas: metadatasToCreate, 
                updatedMetadatas: metadatasToUpdate,
                deletedMetadatas: metadatasToDeleteCopy
            )
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
        _ metadata: ItemMetadata,
        status: ItemMetadata.Status,
        completionHandler: @escaping (_ updatedMetadata: ItemMetadata?) -> Void
    ) {
        guard let result = itemMetadatas.filter("ocId == %@", metadata.ocId).first else {
            Self.logger.debug(
                """
                Did not update status for item metadata as it was not found.
                    ocID: \(metadata.ocId, privacy: .public)
                """
            )
            return
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
                } else if status == .normal {
                    result.chunkUploadId = ""
                }

                Self.logger.debug(
                    """
                    Updated status for item metadata.
                        ocID: \(metadata.ocId, privacy: .public)
                        etag: \(metadata.etag, privacy: .public)
                        fileName: \(metadata.fileName, privacy: .public)
                    """
                )

                completionHandler(ItemMetadata(value: result))
            }
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
            completionHandler(nil)
        }
    }

    public func addItemMetadata(_ metadata: ItemMetadata) {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(ItemMetadata(value: metadata), update: .all)
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
                        lockOwner: \(metadata.lockOwner, privacy: .public)
                        permissions: \(metadata.permissions, privacy: .public)
                        size: \(metadata.size, privacy: .public)
                        trashbinFileName: \(metadata.trashbinFileName, privacy: .public)
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
            let results = itemMetadatas.filter("ocId == %@", ocId)
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
        guard let itemMetadata = itemMetadatas.filter("ocId == %@", ocId).first else {
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
        _ metadata: ItemMetadata
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
