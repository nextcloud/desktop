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
            NSLog("Could not set permission level for File Provider database folder, received error: %@", error.localizedDescription)
        }

        let config = Realm.Configuration(
            fileURL: self.databasePath,
            schemaVersion: self.schemaVersion,
            objectTypes: [NextcloudItemMetadataTable.self, NextcloudDirectoryMetadataTable.self, NextcloudLocalFileMetadataTable.self]
        )

        Realm.Configuration.defaultConfiguration = config

        do {
            _ = try Realm()
            Logger.ncFilesDatabase.info("Successfully started Realm db for FileProviderExt")
        } catch let error as NSError {
            Logger.ncFilesDatabase.error("Error opening Realm db: \(error, privacy: .public)")
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

    private func processItemMetadatasToDelete(databaseToWriteTo: Realm,
                                              existingMetadatas: Results<NextcloudItemMetadataTable>,
                                              updatedMetadatas: [NextcloudItemMetadataTable]) -> [NextcloudItemMetadataTable] {

        assert(databaseToWriteTo.isInWriteTransaction)

        var deletedMetadatas: [NextcloudItemMetadataTable] = []

        for existingMetadata in existingMetadatas {
            guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                    let metadataToDelete = itemMetadataFromOcId(existingMetadata.ocId) else { continue }

            deletedMetadatas.append(metadataToDelete)

            Logger.ncFilesDatabase.debug("Deleting item metadata during update. ocID: \(existingMetadata.ocId, privacy: .public), etag: \(existingMetadata.etag, privacy: .public), fileName: \(existingMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")

            // Can't pass copies, we need the originals from the database
            databaseToWriteTo.delete(ncDatabase().objects(NextcloudItemMetadataTable.self).filter("ocId == %@", metadataToDelete.ocId))
        }

        return deletedMetadatas
    }

    private func processItemMetadatasToUpdate(databaseToWriteTo: Realm,
                                              existingMetadatas: Results<NextcloudItemMetadataTable>,
                                              updatedMetadatas: [NextcloudItemMetadataTable]) -> (newMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable]) {

        assert(databaseToWriteTo.isInWriteTransaction)

        var returningNewMetadatas: [NextcloudItemMetadataTable] = []
        var returningUpdatedMetadatas: [NextcloudItemMetadataTable] = []

        for updatedMetadata in updatedMetadatas {
            if let existingMetadata = existingMetadatas.first(where: { $0.ocId == updatedMetadata.ocId }) {

                if existingMetadata.status == NextcloudItemMetadataTable.Status.normal.rawValue &&
                    !existingMetadata.isInSameDatabaseStoreableRemoteState(updatedMetadata) {

                    returningUpdatedMetadatas.append(NextcloudItemMetadataTable(value: updatedMetadata))
                    databaseToWriteTo.add(updatedMetadata, update: .all)

                    Logger.ncFilesDatabase.debug("Updated existing item metadata. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                } else {
                    Logger.ncFilesDatabase.debug("Skipping item metadata update; same as existing, or still downloading/uploading. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
                }

            } else { // This is a new metadata
                returningNewMetadatas.append(NextcloudItemMetadataTable(value: updatedMetadata))
                databaseToWriteTo.add(updatedMetadata, update: .all)

                Logger.ncFilesDatabase.debug("Created new item metadata during update. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), fileName: \(updatedMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        }

        return (returningNewMetadatas, returningUpdatedMetadatas)
    }

    func updateItemMetadatas(account: String, serverUrl: String, updatedMetadatas: [NextcloudItemMetadataTable], completionHandler: @escaping(_ newMetadatas: [NextcloudItemMetadataTable]?, _ updatedMetadatas: [NextcloudItemMetadataTable]?, _ deletedMetadatas: [NextcloudItemMetadataTable]?) -> Void) {
        let database = ncDatabase()

        do {
            try database.write {
                let existingMetadatas = database.objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@ AND status == %@", account, serverUrl, NextcloudItemMetadataTable.Status.normal.rawValue)

                let deletedMetadatas = processItemMetadatasToDelete(databaseToWriteTo: database,
                                                                    existingMetadatas: existingMetadatas,
                                                                    updatedMetadatas: updatedMetadatas)

                let metadatasFromUpdate = processItemMetadatasToUpdate(databaseToWriteTo: database,
                                                                       existingMetadatas: existingMetadatas,
                                                                       updatedMetadatas: updatedMetadatas)

                completionHandler(metadatasFromUpdate.newMetadatas, metadatasFromUpdate.updatedMetadatas, deletedMetadatas)
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not update any item metadatas, received error: \(error, privacy: .public)")
            completionHandler(nil, nil, nil)
        }
    }

    func setStatusForItemMetadata(_ metadata: NextcloudItemMetadataTable, status: NextcloudItemMetadataTable.Status) -> NextcloudItemMetadataTable? {
        let database = ncDatabase()
        var result: NextcloudItemMetadataTable?

        do {
            try database.write {
                guard let result = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@", metadata.ocId).first else {
                    return
                }

                result.status = status.rawValue
                database.add(result, update: .all)
                Logger.ncFilesDatabase.debug("Updated status for item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not update status for item metadata with ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error, privacy: .public)")
        }

        if result != nil {
            return NextcloudItemMetadataTable(value: result!)
        }

        return nil
    }

    func addItemMetadata(_ metadata: NextcloudItemMetadataTable) {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(metadata, update: .all)
                Logger.ncFilesDatabase.debug("Added item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash))")
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not add item metadata. ocID: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), fileName: \(metadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error, privacy: .public)")
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
            Logger.ncFilesDatabase.error("Could not delete item metadata with ocId: \(ocId, privacy: .public), received error: \(error, privacy: .public)")
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
            Logger.ncFilesDatabase.error("Could not rename filename of item metadata with ocID: \(ocId, privacy: .public) to proposed name \(newFileName, privacy: OSLogPrivacy.auto(mask: .hash)) at proposed serverUrl \(newServerUrl, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error, privacy: .public)")
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

    func directoryMetadata(account: String, serverUrl: String) -> NextcloudDirectoryMetadataTable? {
        if let metadata = ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("account == %@ AND serverUrl == %@", account, serverUrl).first {
            return NextcloudDirectoryMetadataTable(value: metadata)
        }

        return nil
    }

    func directoryMetadata(ocId: String) -> NextcloudDirectoryMetadataTable? {
        if let metadata = ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("ocId == %@", ocId).first {
            return NextcloudDirectoryMetadataTable(value: metadata)
        }

        return nil
    }

    private func sortedDirectoryMetadatas(_ metadatas: Results<NextcloudDirectoryMetadataTable>) -> [NextcloudDirectoryMetadataTable] {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "serverUrl", ascending: true)
        return Array(sortedMetadatas.map { NextcloudDirectoryMetadataTable(value: $0) })
    }

    func childDirectoriesForDirectory(_ directoryMetadata: NextcloudDirectoryMetadataTable) -> [NextcloudDirectoryMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("serverUrl BEGINSWITH %@ AND ocId != %@", directoryMetadata.serverUrl, directoryMetadata.account)
        return sortedDirectoryMetadatas(metadatas)
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: NextcloudItemMetadataTable) -> NextcloudDirectoryMetadataTable? {
        return directoryMetadata(account: itemMetadata.account, serverUrl: itemMetadata.serverUrl)
    }

    func directoryMetadatas(account: String) -> [NextcloudDirectoryMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("account == %@", account)
        return sortedDirectoryMetadatas(metadatas)
    }

    func directoryMetadatas(account: String, parentDirectoryServerUrl: String) -> [NextcloudDirectoryMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("account == %@ AND parentDirectoryServerUrl == %@", account, parentDirectoryServerUrl)
        return sortedDirectoryMetadatas(metadatas)
    }

    private func processDirectoryMetadatasToDelete(databaseToWriteTo: Realm,
                                           existingDirectoryMetadatas: Results<NextcloudDirectoryMetadataTable>,
                                           updatedDirectoryMetadatas: [NextcloudDirectoryMetadataTable]) {

        for existingMetadata in existingDirectoryMetadatas {
            guard !updatedDirectoryMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                  let metadataToDelete = directoryMetadata(ocId: existingMetadata.ocId) else { continue }

            Logger.ncFilesDatabase.debug("Deleting directory metadata during update. ocID: \(existingMetadata.ocId, privacy: .public), etag: \(existingMetadata.etag, privacy: .public), serverUrl: \(existingMetadata.serverUrl)")

            self.deleteDirectoryAndSubdirectoriesMetadata(ocId: metadataToDelete.ocId)
        }
    }

    private func processDirectoryMetadatasToUpdate(databaseToWriteTo: Realm,
                                           existingDirectoryMetadatas: Results<NextcloudDirectoryMetadataTable>,
                                           updatedDirectoryMetadatas: [NextcloudDirectoryMetadataTable]) {

        assert(databaseToWriteTo.isInWriteTransaction)

        for updatedMetadata in updatedDirectoryMetadatas {
            if let existingMetadata = existingDirectoryMetadatas.first(where: { $0.ocId == updatedMetadata.ocId }) {

                if !existingMetadata.isInSameRemoteState(updatedMetadata) {

                    databaseToWriteTo.add(NextcloudDirectoryMetadataTable(value: updatedMetadata), update: .all)
                    Logger.ncFilesDatabase.debug("Updated existing directory metadata. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), serverUrl: \(updatedMetadata.serverUrl)")
                }
                // Don't update under other circumstances in which the metadata already exists

            } else { // This is a new metadata
                databaseToWriteTo.add(NextcloudDirectoryMetadataTable(value: updatedMetadata), update: .all)
                Logger.ncFilesDatabase.debug("Created new directory metadata during update. ocID: \(updatedMetadata.ocId, privacy: .public), etag: \(updatedMetadata.etag, privacy: .public), serverUrl: \(updatedMetadata.serverUrl)")
            }
        }
    }

    func updateDirectoryMetadatas(account: String, parentDirectoryServerUrl: String, updatedDirectoryMetadatas: [NextcloudDirectoryMetadataTable]) {
        let database = ncDatabase()

        let existingDirectoryMetadatas = ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("account == %@ AND parentDirectoryServerUrl == %@", account, parentDirectoryServerUrl)

        // Actual db writing handled internally
        processDirectoryMetadatasToDelete(databaseToWriteTo: database,
                                          existingDirectoryMetadatas: existingDirectoryMetadatas,
                                          updatedDirectoryMetadatas: updatedDirectoryMetadatas)

        do {
            try database.write {

                processDirectoryMetadatasToUpdate(databaseToWriteTo: database,
                                                  existingDirectoryMetadatas: existingDirectoryMetadatas,
                                                  updatedDirectoryMetadatas: updatedDirectoryMetadatas)
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not update directory metadatas, received error: \(error.localizedDescription)")
        }
    }

    func directoryMetadataFromItemMetadata(directoryItemMetadata: NextcloudItemMetadataTable, recordEtag: Bool = false) -> NextcloudDirectoryMetadataTable {
        var newDirectoryMetadata = NextcloudDirectoryMetadataTable()
        let directoryOcId = directoryItemMetadata.ocId

        if let existingDirectoryMetadata = directoryMetadata(ocId: directoryOcId) {
            newDirectoryMetadata = existingDirectoryMetadata
        }

        if recordEtag {
            newDirectoryMetadata.etag = directoryItemMetadata.etag
        }

        newDirectoryMetadata.ocId = directoryOcId
        newDirectoryMetadata.fileId = directoryItemMetadata.fileId
        newDirectoryMetadata.parentDirectoryServerUrl = directoryItemMetadata.serverUrl
        newDirectoryMetadata.serverUrl = directoryItemMetadata.serverUrl + "/" + directoryItemMetadata.fileNameView
        newDirectoryMetadata.account = directoryItemMetadata.account
        newDirectoryMetadata.e2eEncrypted = directoryItemMetadata.e2eEncrypted
        newDirectoryMetadata.favorite = directoryItemMetadata.favorite
        newDirectoryMetadata.permissions = directoryItemMetadata.permissions

        return newDirectoryMetadata
    }

    func updateDirectoryMetadatasFromItemMetadatas(account: String, parentDirectoryServerUrl: String, updatedDirectoryItemMetadatas: [NextcloudItemMetadataTable], recordEtag: Bool = false) {

        var updatedDirMetadatas: [NextcloudDirectoryMetadataTable] = []

        for directoryItemMetadata in updatedDirectoryItemMetadatas {
            let newDirectoryMetadata = directoryMetadataFromItemMetadata(directoryItemMetadata: directoryItemMetadata, recordEtag: recordEtag)
            updatedDirMetadatas.append(newDirectoryMetadata)
        }

        updateDirectoryMetadatas(account: account, parentDirectoryServerUrl: parentDirectoryServerUrl, updatedDirectoryMetadatas: updatedDirMetadatas)
    }

    func addDirectoryMetadata(_ metadata: NextcloudDirectoryMetadataTable) {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(metadata, update: .all)
                Logger.ncFilesDatabase.debug("Added new directory metadata. ocId: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), serverUrl: \(metadata.serverUrl)")
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not add new directory metadata. ocId: \(metadata.ocId, privacy: .public), etag: \(metadata.etag, privacy: .public), serverUrl: \(metadata.serverUrl), received error: \(error, privacy: .public)")
        }
    }

    // Deletes all metadatas related to the info of the directory provided
    func deleteDirectoryAndSubdirectoriesMetadata(ocId: String) {
        let database = ncDatabase()
        guard let directoryMetadata = database.objects(NextcloudDirectoryMetadataTable.self).filter("ocId == %@", ocId).first else {
            Logger.ncFilesDatabase.error("Could not find directory metadata for ocId \(ocId, privacy: .public). Not proceeding with deletion")
            return
        }

        let results = database.objects(NextcloudDirectoryMetadataTable.self).filter("account == %@ AND serverUrl BEGINSWITH %@", directoryMetadata.account, directoryMetadata.serverUrl)

        for result in results {
            deleteItemMetadata(ocId: result.ocId)
            deleteLocalFileMetadata(ocId: result.ocId)
        }

        do {
            try database.write {
                Logger.ncFilesDatabase.debug("Deleting root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryMetadata.etag, privacy: .public), serverUrl: \(directoryMetadata.serverUrl)")
                database.delete(results)
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not delete root directory metadata in recursive delete. ocID: \(directoryMetadata.ocId, privacy: .public), etag: \(directoryMetadata.etag, privacy: .public), serverUrl: \(directoryMetadata.serverUrl), received error: \(error, privacy: .public)")
        }
    }

    func renameDirectoryAndPropagateToChildren(ocId: String, newServerUrl: String, newFileName: String) {

        let database = ncDatabase()

        do {
            try database.write {
                guard let directoryTableResult = database.objects(NextcloudDirectoryMetadataTable.self).filter("ocId == %@", ocId).first,
                      let directoryItemResult = database.objects(NextcloudItemMetadataTable.self).filter("ocId == %@", ocId).first else {
                    Logger.ncFilesDatabase.error("Could not find a directory with ocID \(ocId, privacy: .public), cannot proceed with recursive renaming")
                    return
                }

                let oldServerUrl = directoryTableResult.serverUrl

                let childItemResults = database.objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl BEGINSWITH %@", directoryTableResult.account, oldServerUrl)
                let childDirectoryResults = database.objects(NextcloudDirectoryMetadataTable.self).filter("account == %@ AND serverUrl BEGINSWITH %@", directoryTableResult.account, oldServerUrl)

                directoryTableResult.serverUrl = newServerUrl
                database.add(directoryTableResult, update: .all)
                directoryItemResult.fileName = newFileName
                directoryItemResult.fileNameView = newFileName
                database.add(directoryItemResult, update: .all)
                Logger.ncFilesDatabase.debug("Renamed root renaming directory at \(oldServerUrl) to \(newServerUrl)")

                for childItem in childItemResults {
                    let oldServerUrl = childItem.serverUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(of: oldServerUrl, with: newServerUrl)
                    childItem.serverUrl = movedServerUrl
                    database.add(childItem, update: .all)
                    Logger.ncFilesDatabase.debug("Moved childItem at \(oldServerUrl) to \(movedServerUrl)")
                }

                for childDirectory in childDirectoryResults {
                    let oldServerUrl = childDirectory.serverUrl
                    let oldParentServerUrl = childDirectory.parentDirectoryServerUrl
                    let movedServerUrl = oldServerUrl.replacingOccurrences(of: oldServerUrl, with: newServerUrl)
                    let movedParentServerUrl = oldServerUrl.replacingOccurrences(of: oldParentServerUrl, with: newServerUrl)
                    childDirectory.serverUrl = movedServerUrl
                    childDirectory.parentDirectoryServerUrl = movedParentServerUrl
                    database.add(childDirectory, update: .all)
                    Logger.ncFilesDatabase.debug("Moved childDirectory at \(oldServerUrl) to \(movedServerUrl)")
                }
            }
        } catch let error {
            Logger.ncFilesDatabase.error("Could not rename directory metadata with ocId: \(ocId, privacy: .public) to new serverUrl: \(newServerUrl), received error: \(error, privacy: .public)")
        }
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
            Logger.ncFilesDatabase.error("Could not add local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: OSLogPrivacy.auto(mask: .hash)), received error: \(error, privacy: .public)")
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
            Logger.ncFilesDatabase.error("Could not delete local file metadata with ocId: \(ocId, privacy: .public), received error: \(error, privacy: .public)")
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
