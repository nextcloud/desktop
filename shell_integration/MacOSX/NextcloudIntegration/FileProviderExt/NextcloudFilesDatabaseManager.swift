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

class NextcloudFilesDatabaseManager : NSObject {
    static let shared = {
        return NextcloudFilesDatabaseManager();
    }()

    let relativeDatabaseFolderPath: String = "Database/"
    let databaseFilename: String = "fileproviderextdatabase.realm"
    let relativeDatabaseFilePath: String
    var databasePath: URL?

    let schemaVersion: UInt64 = 100

    override init() {
        self.relativeDatabaseFilePath = self.relativeDatabaseFolderPath + self.databaseFilename

        guard let fileProviderDataDirUrl = pathForFileProviderExtData() else {
            super.init()
            return
        }

        self.databasePath = fileProviderDataDirUrl.appendingPathExtension(self.relativeDatabaseFilePath)

        // Disable file protection for directory DB
        // https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/#std-label-ios-open-a-local-realm
        let folderPath = fileProviderDataDirUrl.appendingPathComponent(self.relativeDatabaseFolderPath).path
        do {
            try FileManager.default.setAttributes([FileAttributeKey.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: folderPath)
        } catch {
            print("Could not set permission level for File Provider database folder")
        }

        let config = Realm.Configuration(
            fileURL: self.databasePath,
            schemaVersion: self.schemaVersion,
            objectTypes: [NextcloudItemMetadataTable.self]
        )

        Realm.Configuration.defaultConfiguration = config

        do {
            let realm = try Realm()
            print("Successfully started Realm db for FileProviderExt")
        } catch let error as NSError {
            print("Error opening Realm db: %@", error.localizedDescription)
        }

        super.init()
    }

    private func ncDatabase() -> Realm {
        let realm = try! Realm()
        realm.refresh()
        return realm
    }

    func itemMetadataFromOcId(_ ocId: String) -> NextcloudItemMetadataTable? {
        return ncDatabase().objects(NextcloudItemMetadataTable.self).filter("ocId == %@", ocId).first
    }

    private func sortedItemMetadatas(_ metadatas: Results<NextcloudItemMetadataTable>) -> [NextcloudItemMetadataTable] {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "fileName", ascending: true)
        return Array(sortedMetadatas.map { $0 })
    }

    func itemMetadatas(account: String, serverUrl: String) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@", account, serverUrl)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadatas(account: String, serverUrl: String, status: NextcloudItemMetadataTable.Status) -> [NextcloudItemMetadataTable] {
        let metadatas = ncDatabase().objects(NextcloudItemMetadataTable.self).filter("account == %@ AND serverUrl == %@ AND status == %@", account, serverUrl, status)
        return sortedItemMetadatas(metadatas)
    }

    func itemMetadataFromFileProviderItemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> NextcloudItemMetadataTable? {
        let ocId = identifier.rawValue
        return itemMetadataFromOcId(ocId)
    }

    func updateItemMetadatas(existingMetadatas: [NextcloudItemMetadataTable], updatedMetadatas: [NextcloudItemMetadataTable]) {
        let database = ncDatabase()

        do {
            try database.write {
                // Delete metadatas
                for existingMetadata in existingMetadatas {
                    guard !updatedMetadatas.contains(where: { $0.ocId == existingMetadata.ocId }),
                            let metadataToDelete = itemMetadataFromOcId(existingMetadata.ocId) else { continue }

                    print("""
                            Deleting metadata.
                            ocID: %@,
                            fileName: %@,
                            etag: %@
                          """
                          , metadataToDelete.ocId, metadataToDelete.fileName, metadataToDelete.etag)
                    database.delete(metadataToDelete)
                }

                // Update existing or create new metadatas
                for updatedMetadata in updatedMetadatas {
                    if let existingMetadata = existingMetadatas.first(where: { $0.ocId == updatedMetadata.ocId }) {

                        if existingMetadata.status == NextcloudItemMetadataTable.Status.normal.rawValue &&
                            !existingMetadata.isInSameRemoteState(updatedMetadata) {

                            database.add(NextcloudItemMetadataTable.init(value: updatedMetadata), update: .all)
                            print("""
                                    Updated existing metadata.
                                    ocID: %@,
                                    fileName: %@,
                                    etag: %@
                                  """
                                  , updatedMetadata.ocId, updatedMetadata.fileName, updatedMetadata.etag)
                        }
                        // Don't update under other circumstances in which the metadata already exists

                    } else { // This is a new metadata
                        database.add(NextcloudItemMetadataTable.init(value: updatedMetadata), update: .all)
                        print("""
                                Created new metadata.
                                ocID: %@,
                                fileName: %@,
                                etag: %@
                              """
                              , updatedMetadata.ocId, updatedMetadata.fileName, updatedMetadata.etag)
                    }


                }
            }
        } catch let error {
            print("Could not update any metadatas, received error: %@", error)
        }
    }

    func directoryMetadata(account: String, serverUrl: String) -> NextcloudDirectoryMetadataTable? {
        return ncDatabase().objects(NextcloudDirectoryMetadataTable.self).filter("account == %@ AND serverUrl == %@", account, serverUrl).first
    }

    func parentDirectoryMetadataForItem(_ itemMetadata: NextcloudItemMetadataTable) -> NextcloudDirectoryMetadataTable? {
        return directoryMetadata(account: itemMetadata.account, serverUrl: itemMetadata.serverUrl)
    }

    func localFileMetadataFromOcId(_ ocId: String) -> NextcloudLocalFileMetadataTable? {
        return ncDatabase().objects(NextcloudLocalFileMetadataTable.self).filter("ocId == %@", ocId).first
    }

    @objc func convertNKFileToItemMetadata(_ file: NKFile, account: String) -> NextcloudItemMetadataTable {

        let metadata = NextcloudItemMetadataTable()

        metadata.account = account
        metadata.checksums = file.checksums
        metadata.commentsUnread = file.commentsUnread
        metadata.contentType = file.contentType
        if let date = file.creationDate {
            metadata.creationDate = date
        } else {
            metadata.creationDate = file.date
        }
        metadata.dataFingerprint = file.dataFingerprint
        metadata.date = file.date
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
        if (metadata.contentType == "text/markdown" || metadata.contentType == "text/x-markdown") && metadata.classFile == NKCommon.typeClassFile.unknow.rawValue {
            metadata.classFile = NKCommon.typeClassFile.document.rawValue
        }
        if let date = file.uploadDate {
            metadata.uploadDate = date
        } else {
            metadata.uploadDate = file.date
        }
        metadata.urlBase = file.urlBase
        metadata.user = file.user
        metadata.userId = file.userId

        // Support for finding the correct filename for e2ee files should go here

        return metadata
    }

    func convertNKFilesToItemMetadatas(_ files: [NKFile], account: String, completionHandler: @escaping (_ directoryMetadata: NextcloudItemMetadataTable, _ childDirectoriesMetadatas: [NextcloudItemMetadataTable], _ metadatas: [NextcloudItemMetadataTable]) -> Void) {

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
