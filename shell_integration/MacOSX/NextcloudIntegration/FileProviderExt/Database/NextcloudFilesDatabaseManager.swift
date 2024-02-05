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
import NextcloudKit
import OSLog
import RealmSwift

class NextcloudFilesDatabaseManager: NSObject {
    static let shared = NextcloudFilesDatabaseManager()

    let relativeDatabaseFolderPath = "Database/"
    let databaseFilename = "fileproviderextdatabase.realm"
    let relativeDatabaseFilePath: String
    var databasePath: URL?

    let schemaVersion: UInt64 = 100

    override init() {
        relativeDatabaseFilePath = relativeDatabaseFolderPath + databaseFilename

        guard let fileProviderDataDirUrl = pathForFileProviderExtData() else {
            super.init()
            return
        }

        databasePath = fileProviderDataDirUrl.appendingPathComponent(relativeDatabaseFilePath)

        // Disable file protection for directory DB
        // https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/#std-label-ios-open-a-local-realm
        let dbFolder = fileProviderDataDirUrl.appendingPathComponent(relativeDatabaseFolderPath)
        let dbFolderPath = dbFolder.path
        do {
            try FileManager.default.createDirectory(at: dbFolder, withIntermediateDirectories: true)
            try FileManager.default.setAttributes(
                [
                    FileAttributeKey.protectionKey: FileProtectionType
                        .completeUntilFirstUserAuthentication
                ],
                ofItemAtPath: dbFolderPath)
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not set permission level for File Provider database folder, received error: \(error.localizedDescription, privacy: .public)"
            )
        }

        let config = Realm.Configuration(
            fileURL: databasePath,
            schemaVersion: schemaVersion,
            objectTypes: [NextcloudItemMetadataTable.self, NextcloudLocalFileMetadataTable.self]
        )

        Realm.Configuration.defaultConfiguration = config

        do {
            _ = try Realm()
            Logger.ncFilesDatabase.info("Successfully started Realm db for FileProviderExt")
        } catch let error as NSError {
            Logger.ncFilesDatabase.error(
                "Error opening Realm db: \(error.localizedDescription, privacy: .public)")
        }

        super.init()
    }

    func ncDatabase() -> Realm {
        let realm = try! Realm()
        realm.refresh()
        return realm
    }

    func anyItemMetadatasForAccount(_ account: String) -> Bool {
        !ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@", account)
            .isEmpty
    }

    func itemMetadataFromOcId(_ ocId: String) -> NextcloudItemMetadataTable? {
        // Realm objects are live-fire, i.e. they will be changed and invalidated according to changes in the db
        // Let's therefore create a copy
        if let itemMetadata = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "ocId == %@", ocId
        ).first {
            return NextcloudItemMetadataTable(value: itemMetadata)
        }

        return nil
    }

    func sortedItemMetadatas(_ metadatas: Results<NextcloudItemMetadataTable>)
        -> [NextcloudItemMetadataTable]
    {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "fileName", ascending: true)
        return Array(sortedMetadatas.map { NextcloudItemMetadataTable(value: $0) })
    }

    func itemMetadatas(account: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "account == %@", account)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadatas(account: String, serverUrl: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND serverUrl == %@", account, serverUrl)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadatas(
        account: String, serverUrl: String, status: NextcloudItemMetadataTable.Status
    )
        -> [NextcloudItemMetadataTable]
    {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
            "account == %@ AND serverUrl == %@ AND status == %@", 
            account,
            serverUrl,
            status.rawValue)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadataFromFileProviderItemIdentifier(_ identifier: NSFileProviderItemIdentifier)
        -> NextcloudItemMetadataTable?
    {
        let ocId = identifier.rawValue
        return itemMetadataFromOcId(ocId)
    }

    private func processItemMetadatasToDelete(
        existingMetadatas: Results<NextcloudItemMetadataTable>,
        updatedMetadatas: [NextcloudItemMetadataTable]
    ) -> [NextcloudItemMetadataTable] {
        var deletedMetadatas: [NextcloudItemMetadataTable] = []

        for existingMetadata in existingMetadatas {
            guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                let metadataToDelete = itemMetadataFromOcId(existingMetadata.ocId)
            else { continue }

            deletedMetadatas.append(metadataToDelete)

            Logger.ncFilesDatabase.debug(
                "Deleting item metadata during update. ocID: \(existingMetadata.ocId, privacy: .public), etag: \(existingMetadata.etag, privacy: .public), fileName: \(existingMetadata.fileName, privacy: .public)"
            )
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(
        existingMetadatas: Results<NextcloudItemMetadataTable>,
        updatedMetadatas: [NextcloudItemMetadataTable],
        updateDirectoryEtags: Bool
    ) -> (
        newMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable],
        directoriesNeedingRename: [NextcloudItemMetadataTable]
    ) {
        var returningNewMetadatas: [NextcloudItemMetadataTable] = []
        var returningUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var directoriesNeedingRename: [NextcloudItemMetadataTable] = []

        for updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingMetadatas.first(where: {
                $0.ocId == updatedMetadata.ocId
            }) {
                if existingMetadata.status == NextcloudItemMetadataTable.Status.normal.rawValue,
                    !existingMetadata.isInSameDatabaseStoreableRemoteState(updatedMetadata)
                {
                    if updatedMetadata.directory {
                        if updatedMetadata.serverUrl != existingMetadata.serverUrl
                            || updatedMetadata.fileName != existingMetadata.fileName
                        {
                            directoriesNeedingRename.append(
                                NextcloudItemMetadataTable(value: updatedMetadata))
                            updatedMetadata.etag = ""  // Renaming doesn't change the etag so reset manually

                        } else if !updateDirectoryEtags {
                            updatedMetadata.etag = existingMetadata.etag
                        }
                    }

                    returningUpdatedMetadatas.append(updatedMetadata)

                    Logger.ncFilesDatabase.debug(
                        "Updated existing item metadata. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: .public)"
                    )
                } else {
                    Logger.ncFilesDatabase.debug(
                        "Skipping item metadata update; same as existing, or still downloading/uploading. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: .public)"
                    )
                }

            } else {  // This is a new metadata
                if !updateDirectoryEtags, updatedMetadata.directory {
                    updatedMetadata.etag = ""
                }

                returningNewMetadatas.append(updatedMetadata)

                Logger.ncFilesDatabase.debug(
                    "Created new item metadata during update. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: .public)"
                )
            }
        }

        return (returningNewMetadatas, returningUpdatedMetadatas, directoriesNeedingRename)
    }

    func updateItemMetadatas(
        account: String, 
        serverUrl: String,
        updatedMetadatas: [NextcloudItemMetadataTable],
        updateDirectoryEtags: Bool
    ) -> (
        newMetadatas: [NextcloudItemMetadataTable]?,
        updatedMetadatas: [NextcloudItemMetadataTable]?,
        deletedMetadatas: [NextcloudItemMetadataTable]?
    ) {
        let database = ncDatabase()

        do {
            let existingMetadatas = database.objects(NextcloudItemMetadataTable.self).filter(
                "account == %@ AND serverUrl == %@ AND status == %@", 
                account,
                serverUrl,
                NextcloudItemMetadataTable.Status.normal.rawValue)

            let metadatasToDelete = processItemMetadatasToDelete(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedMetadatas)

            let metadatasToChange = processItemMetadatasToUpdate(
                existingMetadatas: existingMetadatas,
                updatedMetadatas: updatedMetadatas,
                updateDirectoryEtags: updateDirectoryEtags)

            var metadatasToUpdate = metadatasToChange.updatedMetadatas
            let metadatasToCreate = metadatasToChange.newMetadatas
            let directoriesNeedingRename = metadatasToChange.directoriesNeedingRename

            let metadatasToAdd =
                Array(metadatasToUpdate.map { NextcloudItemMetadataTable(value: $0) })
                + Array(metadatasToCreate.map { NextcloudItemMetadataTable(value: $0) })

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
                for metadata in metadatasToDelete {
                    // Can't pass copies, we need the originals from the database
                    database.delete(
                        ncDatabase().objects(NextcloudItemMetadataTable.self).filter(
                            "ocId == %@", metadata.ocId))
                }

                for metadata in metadatasToAdd {
                    database.add(metadata, update: .all)
                }
            }

            return (
                newMetadatas: metadatasToCreate, 
                updatedMetadatas: metadatasToUpdate,
                deletedMetadatas: metadatasToDelete
            )
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not update any item metadatas, received error: \(error.localizedDescription, privacy: .public)"
            )
            return (nil, nil, nil)
        }
    }

    func setStatusForItemMetadata(
        _ metadata: NextcloudItemMetadataTable, 
        status: NextcloudItemMetadataTable.Status,
        completionHandler: @escaping (_ updatedMetadata: NextcloudItemMetadataTable?) -> Void
    ) {
        let database = ncDatabase()

        do {
            try database.write {
                guard
                    let result = database.objects(NextcloudItemMetadataTable.self).filter(
                        "ocId == %@", metadata.ocId
                    ).first
                else {
                    Logger.ncFilesDatabase.debug(
                        "Did not update status for item metadata as it was not found. ocID: \(metadata.ocId, privacy: .public)"
                    )
                    return
                }

                result.status = status.rawValue
                database.add(result, update: .all)
                Logger.ncFilesDatabase.debug(
                    "Updated status for item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: .public)"
                )

                completionHandler(NextcloudItemMetadataTable(value: result))
            }
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not update status for item metadata with ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
            completionHandler(nil)
        }
    }

    func addItemMetadata(_ metadata: NextcloudItemMetadataTable) {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(metadata, update: .all)
                Logger.ncFilesDatabase.debug(
                    "Added item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: .public)"
                )
            }
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not add item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    @discardableResult func deleteItemMetadata(ocId: String) -> Bool {
        let database = ncDatabase()

        do {
            try database.write {
                let results = database.objects(NextcloudItemMetadataTable.self).filter(
                    "ocId == %@", ocId)

                Logger.ncFilesDatabase.debug("Deleting item metadata. \(ocId, privacy: .public)")
                database.delete(results)
            }

            return true
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not delete item metadata with ocId: \(ocId, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
            return false
        }
    }

    func renameItemMetadata(ocId: String, newServerUrl: String, newFileName: String) {
        let database = ncDatabase()

        do {
            try database.write {
                guard
                    let itemMetadata = database.objects(NextcloudItemMetadataTable.self).filter(
                        "ocId == %@", ocId
                    ).first
                else {
                    Logger.ncFilesDatabase.debug(
                        "Could not find an item with ocID \(ocId, privacy: .public) to rename to \(newFileName, privacy: .public)"
                    )
                    return
                }

                let oldFileName = itemMetadata.fileName
                let oldServerUrl = itemMetadata.serverUrl

                itemMetadata.fileName = newFileName
                itemMetadata.fileNameView = newFileName
                itemMetadata.serverUrl = newServerUrl

                database.add(itemMetadata, update: .all)

                Logger.ncFilesDatabase.debug(
                    "Renamed item \(oldFileName, privacy: .public) to \(newFileName, privacy: .public), moved from serverUrl: \(oldServerUrl, privacy: .public) to serverUrl: \(newServerUrl, privacy: .public)"
                )
            }
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not rename filename of item metadata with ocID: \(ocId, privacy: .public) to proposed name \(newFileName, privacy: .public) at proposed serverUrl \(newServerUrl, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    func parentItemIdentifierFromMetadata(_ metadata: NextcloudItemMetadataTable)
        -> NSFileProviderItemIdentifier?
    {
        let homeServerFilesUrl = metadata.urlBase + "/remote.php/dav/files/" + metadata.userId

        if metadata.serverUrl == homeServerFilesUrl {
            return .rootContainer
        }

        guard let itemParentDirectory = parentDirectoryMetadataForItem(metadata) else {
            Logger.ncFilesDatabase.error(
                "Could not get item parent directory metadata for metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: .public)"
            )
            return nil
        }

        if let parentDirectoryMetadata = itemMetadataFromOcId(itemParentDirectory.ocId) {
            return NSFileProviderItemIdentifier(parentDirectoryMetadata.ocId)
        }

        Logger.ncFilesDatabase.error(
            "Could not get item parent directory item metadata for metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: .public)"
        )
        return nil
    }
}
