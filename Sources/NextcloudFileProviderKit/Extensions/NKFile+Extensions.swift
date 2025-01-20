//
//  NKFile+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

import Foundation
import NextcloudKit
import RealmSwift

extension NKFile {
    func toItemMetadata() -> ItemMetadata {
        let metadata = ItemMetadata()

        metadata.account = account
        metadata.checksums = checksums
        metadata.commentsUnread = commentsUnread
        metadata.contentType = contentType
        if let creationDate {
            metadata.creationDate = creationDate as Date
        } else {
            metadata.creationDate = date as Date
        }
        metadata.dataFingerprint = dataFingerprint
        metadata.date = date as Date
        metadata.directory = directory
        metadata.downloadURL = downloadURL
        metadata.e2eEncrypted = e2eEncrypted
        metadata.etag = etag
        metadata.favorite = favorite
        metadata.fileId = fileId
        metadata.fileName = fileName
        metadata.fileNameView = fileName
        metadata.hasPreview = hasPreview
        metadata.iconName = iconName
        metadata.mountType = mountType
        metadata.name = name
        metadata.note = note
        metadata.ocId = ocId
        metadata.ownerId = ownerId
        metadata.ownerDisplayName = ownerDisplayName
        metadata.lock = lock
        metadata.lockOwner = lockOwner
        metadata.lockOwnerEditor = lockOwnerEditor
        metadata.lockOwnerType = lockOwnerType
        metadata.lockOwnerDisplayName = lockOwnerDisplayName
        metadata.lockTime = lockTime
        metadata.lockTimeOut = lockTimeOut
        metadata.path = path
        metadata.permissions = permissions
        metadata.quotaUsedBytes = quotaUsedBytes
        metadata.quotaAvailableBytes = quotaAvailableBytes
        metadata.richWorkspace = richWorkspace
        metadata.resourceType = resourceType
        metadata.serverUrl = serverUrl
        metadata.sharePermissionsCollaborationServices = sharePermissionsCollaborationServices
        for element in sharePermissionsCloudMesh {
            metadata.sharePermissionsCloudMesh.append(element)
        }
        for element in shareType {
            metadata.shareType.append(element)
        }
        metadata.size = size
        metadata.classFile = classFile
        // FIXME: iOS 12.0,* don't detect UTI text/markdown, text/x-markdown
        if metadata.contentType == "text/markdown" || metadata.contentType == "text/x-markdown",
            metadata.classFile == NKCommon.TypeClassFile.unknow.rawValue
        {
            metadata.classFile = NKCommon.TypeClassFile.document.rawValue
        }
        if let uploadDate {
            metadata.uploadDate = uploadDate as Date
        } else {
            metadata.uploadDate = date as Date
        }
        metadata.trashbinFileName = trashbinFileName
        metadata.trashbinOriginalLocation = trashbinOriginalLocation
        metadata.trashbinDeletionTime = trashbinDeletionTime
        metadata.urlBase = urlBase
        metadata.user = user
        metadata.userId = userId

        // Support for finding the correct filename for e2ee files should go here

        return metadata
    }
}



extension Array<NKFile> {
    /// Realm objects are inherently unsendable and not thread-safe **IF THEY ARE MANAGED.**
    /// Marking our ItemMetadata as an unchecked Sendable is a naughty thing to do. So make sure to check
    /// for ItemMetadata objects to be unmanaged before doing anything crossing actor boundaries.
    private class SendableItemMetadata: ItemMetadata, @unchecked Sendable {}

    private final actor DirectoryReadConversionActor: Sendable {
        let directoryMetadata: SendableItemMetadata
        var childDirectoriesMetadatas: [SendableItemMetadata] = []
        var metadatas: [SendableItemMetadata] = []

        func convertedMetadatas() -> (SendableItemMetadata, [SendableItemMetadata], [SendableItemMetadata]) {
            (directoryMetadata, childDirectoriesMetadatas, metadatas)
        }

        init(target: SendableItemMetadata) {
            self.directoryMetadata = target
        }

        func add(metadata: SendableItemMetadata) {
            assert(metadata.realm == nil, "Realm objects used in actor context should be unmanaged")
            metadatas.append(metadata)
            if metadata.directory {
                childDirectoriesMetadatas.append(metadata)
            }
        }
    }

    func toDirectoryReadMetadatas(account: Account) async -> (
        directoryMetadata: ItemMetadata,
        childDirectoriesMetadatas: [ItemMetadata],
        metadatas: [ItemMetadata]
    ) {
        guard let targetDirectoryMetadata = first?.toItemMetadata() else {
            return (ItemMetadata(), [], [])
        }
        let conversionActor =
            DirectoryReadConversionActor(target: SendableItemMetadata(value: targetDirectoryMetadata))
        await concurrentChunkedForEach { file in
            guard file.ocId != targetDirectoryMetadata.ocId else { return }
            await conversionActor.add(metadata: SendableItemMetadata(value: file.toItemMetadata()))
        }
        return await conversionActor.convertedMetadatas()
    }
}
