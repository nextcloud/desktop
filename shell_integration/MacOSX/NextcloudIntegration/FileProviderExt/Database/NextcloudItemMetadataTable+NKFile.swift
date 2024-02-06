/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import NextcloudKit

extension NextcloudItemMetadataTable {
    static func fromNKFile(_ file: NKFile, account: String) -> NextcloudItemMetadataTable {
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
        // FIXME: iOS 12.0,* don't detect UTI text/markdown, text/x-markdown
        if metadata.contentType == "text/markdown" || metadata.contentType == "text/x-markdown",
            metadata.classFile == NKCommon.TypeClassFile.unknow.rawValue
        {
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

    static func metadatasFromDirectoryReadNKFiles(
        _ files: [NKFile],
        account: String,
        completionHandler: @escaping (
            _ directoryMetadata: NextcloudItemMetadataTable,
            _ childDirectoriesMetadatas: [NextcloudItemMetadataTable],
            _ metadatas: [NextcloudItemMetadataTable]
        ) -> Void
    ) {
        var directoryMetadataSet = false
        var directoryMetadata = NextcloudItemMetadataTable()
        var childDirectoriesMetadatas: [NextcloudItemMetadataTable] = []
        var metadatas: [NextcloudItemMetadataTable] = []

        let conversionQueue = DispatchQueue(
            label: "nkFileToMetadataConversionQueue", 
            qos: .userInitiated,
            attributes: .concurrent)
        // appendQueue is a serial queue, not concurrent
        let appendQueue = DispatchQueue(label: "metadataAppendQueue", qos: .userInitiated)
        let dispatchGroup = DispatchGroup()

        for file in files {
            if metadatas.isEmpty, !directoryMetadataSet {
                let metadata = NextcloudItemMetadataTable.fromNKFile(file, account: account)
                directoryMetadata = metadata
                directoryMetadataSet = true
            } else {
                conversionQueue.async(group: dispatchGroup) {
                    let metadata = NextcloudItemMetadataTable.fromNKFile(file, account: account)

                    appendQueue.async(group: dispatchGroup) {
                        metadatas.append(metadata)
                        if metadata.directory {
                            childDirectoriesMetadatas.append(metadata)
                        }
                    }
                }
            }
        }

        dispatchGroup.notify(queue: DispatchQueue.main) {
            completionHandler(directoryMetadata, childDirectoriesMetadatas, metadatas)
        }
    }
}
