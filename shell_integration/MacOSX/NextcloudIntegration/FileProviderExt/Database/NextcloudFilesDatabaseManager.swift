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

import Foundation
import RealmSwift
import FileProvider
import NextcloudKit
import OSLog

class NextcloudFilesDatabaseManager : NSObject {
    static let shared = {
        return NextcloudFilesDatabaseManager();
    }()

    let relativeDatabaseFolderPath = "Database/"
    let databaseFilename = "fileproviderextdatabase.realm"
    let relativeDatabaseFilePath: String
    var databasePath: URL?

    let schemaVersion: UInt64 = 100

    override init() {
        self.relativeDatabaseFilePath = self.relativeDatabaseFolderPath + self.databaseFilename

        guard let fileProviderDataDirUrl = pathForFileProviderExtData() else {
            super.init()
            return
        }

        self.databasePath = fileProviderDataDirUrl.appendingPathComponent(self.relativeDatabaseFilePath)

        // Disable file protection for directory DB
        // https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/#std-label-ios-open-a-local-realm
        let dbFolder = fileProviderDataDirUrl.appendingPathComponent(self.relativeDatabaseFolderPath)
        let dbFolderPath = dbFolder.path
        do {
            try FileManager.default.createDirectory(at: dbFolder, withIntermediateDirectories: true)
            try FileManager.default.setAttributes([FileAttributeKey.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: dbFolderPath)
        } catch let error {
            Logger.ncFilesDatabase.error("Could not set permission level for File Provider database folder, received error: \(error.localizedDescription, privacy: .public)")
        }

        let config = Realm.Configuration(
            fileURL: self.databasePath,
            schemaVersion: self.schemaVersion,
            objectTypes: [NextcloudItemMetadataTable.self, NextcloudLocalFileMetadataTable.self]
        )

        Realm.Configuration.defaultConfiguration = config

        do {
            _ = try Realm()
            Logger.ncFilesDatabase.info("Successfully started Realm db for FileProviderExt")
        } catch let error as NSError {
            Logger.ncFilesDatabase.error("Error opening Realm db: \(error.localizedDescription, privacy: .public)")
        }

        super.init()
    }

    private func ncDatabase() -> Realm {
        let realm = try! Realm()
        realm.refresh()
        return realm
    }

    func anyItemMetadatasForAccount(_ account: String) -> Bool {
        return !ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@", account).isEmpty
    }

    func itemMetadataFromOcId(_ ocId: String) -> NextcloudItemMetadataTable? {
        // Realm objects are live-fire, i.e. they will be changed and invalidated according to changes in the db
        // Let's therefore create a copy
        if let itemMetadata = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("ocId == %@", ocId).first {
            return NextcloudItemMetadataTable(value: itemMetadata)
        }

        return nil
    }

    private func sortedItemMetadatas(_ metadatas: Results<NextcloudItemMetadataTable>) -> [NextcloudItemMetadataTable] {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "fileName", ascending: true)
        return Array(sortedMetadatas.map { NextcloudItemMetadataTable(value: $0) })
    }

    func itemMetadatas(account: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@", account)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadatas(account: String, serverUrl: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@", account, serverUrl)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadatas(account: String, serverUrl: String, status: NextcloudItemMetadataTable.Status) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@ AND status == %@", account, serverUrl, status.rawValue)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadataFromFileProviderItemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> NextcloudItemMetadataTable? {
        let ocId = identifier.rawValue
        return itemMetadataFromOcId(ocId)
    }

    private func processItemMetadatasToDelete(existingMetadatas: Results<NextcloudItemMetadataTable>,
                                              updatedMetadatas: [NextcloudItemMetadataTable]) -> [NextcloudItemMetadataTable] {

        var deletedMetadatas: [NextcloudItemMetadataTable] = []

        for existingMetadata in existingMetadatas {
            guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                    let metadataToDelete = itemMetadataFromOcId(existingMetadata.ocId) else { continue }

            deletedMetadatas.append(metadataToDelete)

            Logger.ncFilesDatabase.debug("Deleting item metadata during update. ocID: \(existingMetadata.ocId, privacy: .public), etag: \(existingMetadata.etag, privacy: .public), fileName: \(existingMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(existingMetadatas: Results<NextcloudItemMetadataTable>,
                                              updatedMetadatas: [NextcloudItemMetadataTable],
                                              updateDirectoryEtags: Bool) -> (newMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable], directoriesNeedingRename: [NextcloudItemMetadataTable]) {

        var returningNewMetadatas: [NextcloudItemMetadataTable] = []
        var returningUpdatedMetadatas: [NextcloudItemMetadataTable] = []
        var directoriesNeedingRename: [NextcloudItemMetadataTable] = []

        for updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingMetadatas.first(where: { $0.ocId == updatedMetadata.ocId }) {

                if existingMetadata.status == NextcloudItemMetadataTable.Status.normal.rawValue &&
                    !existingMetadata.isInSameDatabaseStoreableRemoteState(updatedMetadata) {

                    if updatedMetadata.directory {

                        if updatedMetadata.serverUrl != existingMetadata.serverUrl || updatedMetadata.fileName != existingMetadata.fileName {

                            directoriesNeedingRename.append(NextcloudItemMetadataTable(value: updatedMetadata))
                            updatedMetadata.etag = "" // Renaming doesn't change the etag so reset manually

                        } else if !updateDirectoryEtags {
                            updatedMetadata.etag = existingMetadata.etag
                        }
                    }

                    returningUpdatedMetadatas.append(updatedMetadata)


                    Logger.ncFilesDatabase.debug("Updated existing item metadata. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                } else {
                    Logger.ncFilesDatabase.debug("Skipping item metadata update; same as existing, or still downloading/uploading. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                }

            } else { // This is a new metadata
                returningNewMetadatas.append(updatedMetadata)

                Logger.ncFilesDatabase.debug("Created new item metadata during update. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        }

        return (returningNewMetadatas, returningUpdatedMetadatas, directoriesNeedingRename)
    }

    func updateItemMetadatas(account: String, serverUrl: String, updatedMetadatas: [NextcloudItemMetadataTable], updateDirectoryEtags: Bool, completionHandler: @escaping(_ newMetadatas: [NextcloudItemMetadataTable]?, _ updatedMetadatas: [NextcloudItemMetadataTable]?, _ deletedMetadatas: [NextcloudItemMetadataTable]?) -> Void) {
        let database = ncDatabase()

        do {
            let existingMetadatas = database.objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@ AND status == %@", account, serverUrl, NextcloudItemMetadataTable.Status.normal.rawValue)

            let metadatasToDelete = processItemMetadatasToDelete(existingMetadatas: existingMetadatas,
                                                                 updatedMetadatas: updatedMetadatas)

            let metadatasToChange = processItemMetadatasToUpdate(existingMetadatas: existingMetadatas,
                                                                 updatedMetadatas: updatedMetadatas,
                                                                 updateDirectoryEtags: updateDirectoryEtags)

            var metadatasToUpdate = metadatasToChange.updatedMetadatas
            let metadatasToCreate = metadatasToChange.newMetadatas
            let directoriesNeedingRename = metadatasToChange.directoriesNeedingRename

            let metadatasToAdd = Array(metadatasToUpdate.map { NextcloudItemMetadataTable(value: $0) }) +
                                 Array(metadatasToCreate.map { NextcloudItemMetadataTable(value: $0) })

            try database.write {
                for metadata in metadatasToDelete {
                    // Can't pass copies, we need the originals from the database
                    database.delete(ncDatabase().objects(NextcloudItemMetadataTable.self).filter("ocId == %@", metadata.ocId))
                }

                for metadata in metadatasToAdd {
                    database.add(metadata, update: .all)
                }

            }

            for metadata in directoriesNeedingRename {
                if let updatedDirectoryChildren = renameDirectoryAndPropagateToChildren(ocId: metadata.ocId, newServerUrl: metadata.serverUrl, newFileName: metadata.fileName) {
                    metadatasToUpdate += updatedDirectoryChildren
                }
            }

            completionHandler(metadatasToCreate, metadatasToUpdate, metadatasToDelete)
        } catch let error {
            Logger.ncFilesDatabase.error("Could not update any item metadatas, received error: \(error.localizedDescription, privacy: .public)")
            completionHandler(nil, nil, nil)
        }
    }

    func setStatusForItemMetadata(_ metadata: NextcloudItemMetadataTable, status: NextcloudItemMetadataTable.Status, completionHandler: @escaping(_ updatedMetadata: NextcloudItemMetadataTable?) -> Void) {
        let database = ncDatabase()

        do {
            try database.write {
                guard let result = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@", metadata.ocId).first else {
                    Logger.ncFilesDatabase.debug("Did not update status for item metadata as it was not found. ocID: \(metadata.ocId, privacy: .public)")
                    return
                }

                result.status = status.rawValue
                database.add(result, update: .all)
                Logger.ncFilesDatabase.debug("Updated status for item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")

                completionHandler(NextcloudItemMetadataTable(value: result))
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not update status for item metadata with ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error.localizedDescription, privacy: .public)")
            completionHandler(nil)
        }
    }

    func addItemMetadata(_ metadata: NextcloudItemMetadataTable) {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(metadata, update: .all)
                Logger.ncFilesDatabase.debug("Added item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not add item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error.localizedDescription, privacy: .public)")
        }
    }

    func deleteItemMetadata(ocId: String) {
        let database = ncDatabase()

        do {
            try database.write {
                let results = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@", ocId)

                Logger.ncFilesDatabase.debug("Deleting item metadata. \(ocId, privacy: .public)")
                database.delete(results)
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not delete item metadata with ocId: \(ocId, privacy: .public), received error: \(error.localizedDescription, privacy: .public)")
        }
    }

    func renameItemMetadata(ocId: String, newServerUrl: String, newFileName: String) {
        let database = ncDatabase()

        do {
            try database.write {
                guard let itemMetadata = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@", ocId).first else {
                    Logger.ncFilesDatabase.debug("Could not find an item with ocID \(ocId, privacy: .public) to rename to \(newFileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                    return
                }

                let oldFileName = itemMetadata.fileName
                let oldServerUrl = itemMetadata.serverUrl

                itemMetadata.fileName = newFileName
                itemMetadata.fileNameView = newFileName
                itemMetadata.serverUrl = newServerUrl

                database.add(itemMetadata, update: .all)

                Logger.ncFilesDatabase.debug("Renamed item \(oldFileName, privacy: OSLogPrivacy.auto(mask: .hash)) to \(newFileName, privacy: OSLogPrivacy.auto(mask: .hash)), moved from serverUrl: \(oldServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)) to serverUrl: \(newServerUrl, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not rename filename of item metadata with ocID: \(ocId, privacy: .public) to proposed name \(newFileName, privacy: OSLogPrivacy.auto(mask: .hash)) at proposed serverUrl \(newServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error.localizedDescription, privacy: .public)")
        }
    }

    func parentItemIdentifierFromMetadata(_ metadata: NextcloudItemMetadataTable) -> NSFileProviderItemIdentifier? {
        let homeServerFilesUrl = metadata.urlBase + "/remote.php/dav/files/" + metadata.userId

        if metadata.serverUrl == homeServerFilesUrl {
            return .rootContainer
        }

        guard let itemParentDirectory = parentDirectoryMetadataForItem(metadata) else {
            Logger.ncFilesDatabase.error("Could not get item parent directory metadata for metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            return nil
        }

        if let parentDirectoryMetadata = itemMetadataFromOcId(itemParentDirectory.ocId) {
            return NSFileProviderItemIdentifier(parentDirectoryMetadata.ocId)
        }

        Logger.ncFilesDatabase.error("Could not get item parent directory item metadata for metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
        return nil
    }

    func directoryMetadata(account: String, serverUrl: String) -> NextcloudItemMetadataTable? {
        // We want to split by "/" (e.g. cloud.nc.com/files/a/b) but we need to be mindful of "https://c.nc.com"
        let problematicSeparator = "://"
        let placeholderSeparator = "__TEMP_REPLACE__"
        let serverUrlWithoutPrefix = serverUrl.replacingOccurrences(of: problematicSeparator, with: placeholderSeparator)
        var splitServerUrl = serverUrlWithoutPrefix.split(separator: "/")
        let directoryItemFileName = String(splitServerUrl.removeLast())
        let directoryItemServerUrl = splitServerUrl.joined(separator: "/").replacingOccurrences(of: placeholderSeparator, with: problematicSeparator)

        if let metadata = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@ AND fileName == %@ AND directory == true", account, directoryItemServerUrl, directoryItemFileName).first {
            return NextcloudItemMetadataTable(value: metadata)
        }

        return nil
    }

    func childDirectoriesForDirectory(_ directoryMetadata: NextcloudItemMetadataTable) -> [NextcloudItemMetadataTable] {
        let directoryServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("serverUrl BEGINSWITH %@ AND ocId != %@ AND directory == true", directoryServerUrl, directoryMetadata.account)
        return sortedItemMetadatas(metadatas)
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: NextcloudItemMetadataTable) -> NextcloudItemMetadataTable? {
        return directoryMetadata(account: itemMetadata.account, serverUrl: itemMetadata.serverUrl)
    }

    func directoryMetadata(ocId: String) -> NextcloudItemMetadataTable? {
        if let metadata = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("ocId == %@ AND directory == true", ocId).first {
            return NextcloudItemMetadataTable(value: metadata)
        }

        return nil
    }

    func directoryMetadatas(account: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND directory == true", account)
        return sortedItemMetadatas(metadatas)
    }

    func directoryMetadatas(account: String, parentDirectoryServerUrl: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND parentDirectoryServerUrl == %@ AND directory == true", account, parentDirectoryServerUrl)
        return sortedItemMetadatas(metadatas)
    }

    // Deletes all metadatas related to the info of the directory provided
    func deleteDirectoryAndSubdirectoriesMetadata(ocId: String) {
        let database = ncDatabase()
        guard let directoryMetadata = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@ AND directory == true", ocId).first else {
            Logger.ncFilesDatabase.error("Could not find directory metadata for ocId \(ocId, privacy: .public). Not proceeding with deletion")
            return
        }

        let directoryUrlPath = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let results = database.objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account, directoryUrlPath)

        for result in results {
            deleteItemMetadata(ocId: result.ocId)
            deleteLocalFileMetadata(ocId: result.ocId)
        }

        do {
            try database.write {
                Logger.ncFilesDatabase.debug("Deleting root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryMetadata.etag, privacy: .public), serverUrl: \(directoryUrlPath)")
                database.delete(results)
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not delete root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryMetadata.etag, privacy: .public), serverUrl: \(directoryUrlPath), received error: \(error.localizedDescription, privacy: .public)")
        }
    }

    func renameDirectoryAndPropagateToChildren(ocId: String, newServerUrl: String, newFileName: String) -> [NextcloudItemMetadataTable]? {

        let database = ncDatabase()

        guard let directoryMetadata = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@ AND directory == true", ocId).first else {
            Logger.ncFilesDatabase.error("Could not find a directory with ocID \(ocId, privacy: .public), cannot proceed with recursive renaming")
            return nil
        }

        let oldServerUrl = directoryMetadata.serverUrl + "/" + directoryMetadata.fileName
        let childItemResults = database.objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account, oldServerUrl)

        renameItemMetadata(ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName)
        Logger.ncFilesDatabase.debug("Renamed root renaming directory")

        do {
            try database.write {
                for childItem in childItemResults {
                    let oldServerUrl = childItem.serverUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(of: oldServerUrl, with: newServerUrl)
                    childItem.serverUrl = movedServerUrl
                    database.add(childItem, update: .all)
                    Logger.ncFilesDatabase.debug("Moved childItem at \(oldServerUrl) to \(movedServerUrl)")
                }
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not rename directory metadata with ocId: \(ocId, privacy: .public) to new serverUrl: \(newServerUrl), received error: \(error.localizedDescription, privacy: .public)")

            return nil
        }

        let updatedChildItemResults = database.objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account, newServerUrl)
        return sortedItemMetadatas(updatedChildItemResults)
    }

    func localFileMetadataFromOcId(_ ocId: String) -> NextcloudLocalFileMetadataTable? {
        if let metadata = ncDatabase().objects(NextcloudLocalFileMetadataTable.self).filter("ocId == %@", ocId).first {
            return NextcloudLocalFileMetadataTable(value: metadata)
        }

        return nil
    }

    func addLocalFileMetadataFromItemMetadata(_ itemMetadata: NextcloudItemMetadataTable) {
        let database = ncDatabase()

        do {
            try database.write {
                let newLocalFileMetadata = NextcloudLocalFileMetadataTable()

                newLocalFileMetadata.ocId = itemMetadata.ocId
                newLocalFileMetadata.fileName = itemMetadata.fileName
                newLocalFileMetadata.account = itemMetadata.account
                newLocalFileMetadata.etag = itemMetadata.etag
                newLocalFileMetadata.exifDate = Date()
                newLocalFileMetadata.exifLatitude = "-1"
                newLocalFileMetadata.exifLongitude = "-1"

                database.add(newLocalFileMetadata, update: .all)
                Logger.ncFilesDatabase.debug("Added local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not add local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error.localizedDescription, privacy: .public)")
        }
    }

    func deleteLocalFileMetadata(ocId: String) {
        let database = ncDatabase()

        do {
            try database.write {
                let results = database.objects(NextcloudLocalFileMetadataTable.self).filter("ocId == %@", ocId)
                database.delete(results)
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not delete local file metadata with ocId: \(ocId, privacy: .public), received error: \(error.localizedDescription, privacy: .public)")
        }
    }

    private func sortedLocalFileMetadatas(_ metadatas: Results<NextcloudLocalFileMetadataTable>) -> [NextcloudLocalFileMetadataTable] {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "fileName", ascending: true)
        return Array(sortedMetadatas.map { NextcloudLocalFileMetadataTable(value: $0) })
    }

    func localFileMetadatas(account: String) -> [NextcloudLocalFileMetadataTable] {
        let results = ncDatabase().objects(NextcloudLocalFileMetadataTable.self).filter("account == %@", account)
        return sortedLocalFileMetadatas(results)
    }

    func localFileItemMetadatas(account: String) -> [NextcloudItemMetadataTable] {
        let localFileMetadatas = localFileMetadatas(account: account)
        let localFileMetadatasOcIds = Array(localFileMetadatas.map { $0.ocId })

        var itemMetadatas: [NextcloudItemMetadataTable] = []

        for ocId in localFileMetadatasOcIds {
            guard let itemMetadata = itemMetadataFromOcId(ocId) else {
                Logger.ncFilesDatabase.error("Could not find matching item metadata for local file metadata with ocId: \(ocId, privacy: .public) with request from account: \(account)")
                continue;
            }

            itemMetadatas.append(NextcloudItemMetadataTable(value: itemMetadata))
        }

        return itemMetadatas
    }

    func convertNKFileToItemMetadata(_ file: NKFile, account: String) -> NextcloudItemMetadataTable {

        let metadata = NextcloudItemMetadataTable()

        metadata.account = account
        metadata.checksums = file.checksums
        metadata.commentsUnread = file.commentsUnread
        metadata.contentType = file.contentType
        if let date = file.creationDate {
            metadata.creationDate = date as Date
        } else {
            metadata.creationDate = file.date as Date
        }
        metadata.dataFingerprint = file.dataFingerprint
        metadata.date = file.date as Date
        metadata.directory = file.directory
        metadata.downloadURL = file.downloadURL
        metadata.e2eEncrypted = file.e2eEncrypted
        metadata.etag = file.etag
        metadata.favorite = file.favorite
        metadata.fileId = file.fileId
        metadata.fileName = file.fileName
        metadata.fileNameView = file.fileName
        metadata.hasPreview = file.hasPreview
        metadata.iconName = file.iconName
        metadata.mountType = file.mountType
        metadata.name = file.name
        metadata.note = file.note
        metadata.ocId = file.ocId
        metadata.ownerId = file.ownerId
        metadata.ownerDisplayName = file.ownerDisplayName
        metadata.lock = file.lock
        metadata.lockOwner = file.lockOwner
        metadata.lockOwnerEditor = file.lockOwnerEditor
        metadata.lockOwnerType = file.lockOwnerType
        metadata.lockOwnerDisplayName = file.lockOwnerDisplayName
        metadata.lockTime = file.lockTime
        metadata.lockTimeOut = file.lockTimeOut
        metadata.path = file.path
        metadata.permissions = file.permissions
        metadata.quotaUsedBytes = file.quotaUsedBytes
        metadata.quotaAvailableBytes = file.quotaAvailableBytes
        metadata.richWorkspace = file.richWorkspace
        metadata.resourceType = file.resourceType
        metadata.serverUrl = file.serverUrl
        metadata.sharePermissionsCollaborationServices = file.sharePermissionsCollaborationServices
        for element in file.sharePermissionsCloudMesh {
            metadata.sharePermissionsCloudMesh.append(element)
        }
        for element in file.shareType {
            metadata.shareType.append(element)
        }
        metadata.size = file.size
        metadata.classFile = file.classFile
        //FIXME: iOS 12.0,* don't detect UTI text/markdown, text/x-markdown
        if (metadata.contentType == "text/markdown" || metadata.contentType == "text/x-markdown") && metadata.classFile == NKCommon.TypeClassFile.unknow.rawValue {
            metadata.classFile = NKCommon.TypeClassFile.document.rawValue
        }
        if let date = file.uploadDate {
            metadata.uploadDate = date as Date
        } else {
            metadata.uploadDate = file.date as Date
        }
        metadata.urlBase = file.urlBase
        metadata.user = file.user
        metadata.userId = file.userId

        // Support for finding the correct filename for e2ee files should go here

        return metadata
    }

    func convertNKFilesFromDirectoryReadToItemMetadatas(_ files: [NKFile], account: String, completionHandler: @escaping (_ directoryMetadata: NextcloudItemMetadataTable, _ childDirectoriesMetadatas: [NextcloudItemMetadataTable], _ metadatas: [NextcloudItemMetadataTable]) -> Void) {

        var directoryMetadataSet = false
        var directoryMetadata = NextcloudItemMetadataTable()
        var childDirectoriesMetadatas: [NextcloudItemMetadataTable] = []
        var metadatas: [NextcloudItemMetadataTable] = []

        for file in files {
            let metadata = convertNKFileToItemMetadata(file, account: account)

            if metadatas.isEmpty && !directoryMetadataSet {
                directoryMetadata = metadata;
                directoryMetadataSet = true;
            } else {
                metadatas.append(metadata)
                if metadata.directory {
                    childDirectoriesMetadatas.append(metadata)
                }
            }
        }

        completionHandler(directoryMetadata, childDirectoriesMetadatas, metadatas)
    }
}
