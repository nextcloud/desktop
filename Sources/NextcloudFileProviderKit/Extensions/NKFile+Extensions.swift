//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudKit
import RealmSwift

extension NKFile {
    func fullUrlMatches(_ urlString: String) -> Bool {
        var fileUrl = serverUrl + "/" + fileName
        if fileUrl.last == "/" { // This is likely the root container, as it has no filename
            fileUrl.removeLast()
        }
        return fileUrl == urlString
    }

    func toItemMetadata(uploaded: Bool = true) -> SendableItemMetadata {
        let creationDate = creationDate ?? date
        let uploadDate = uploadDate ?? date
        let classFile = (contentType == "text/markdown" || contentType == "text/x-markdown")
            && classFile == NKTypeClassFile.unknow.rawValue
                ? NKTypeClassFile.document.rawValue
                : classFile
        // Support for finding the correct filename for e2ee files should go here

        // Don't ask me why, NextcloudKit renames and moves the root folder details
        // Also don't ask me why, but, NextcloudKit marks the NKFile for this as not a directory
        let rootRequiresFixup = serverUrl == ".." && fileName == "."
        let ocId = rootRequiresFixup
            ? NSFileProviderItemIdentifier.rootContainer.rawValue
            : self.ocId
        let directory = rootRequiresFixup ? true : self.directory
        let serverUrl = rootRequiresFixup
            ? urlBase + Account.webDavFilesUrlSuffix + userId
            : self.serverUrl
        let fileName = rootRequiresFixup ? "" : self.fileName

        return SendableItemMetadata(
            ocId: ocId,
            account: account,
            checksums: checksums,
            classFile: classFile,
            commentsUnread: commentsUnread,
            contentType: contentType,
            creationDate: creationDate as Date,
            dataFingerprint: dataFingerprint,
            date: date as Date,
            directory: directory,
            downloadURL: downloadURL,
            e2eEncrypted: e2eEncrypted,
            etag: etag,
            favorite: favorite,
            fileId: fileId,
            fileName: fileName,
            fileNameView: fileName,
            hasPreview: hasPreview,
            hidden: hidden,
            iconName: iconName,
            livePhotoFile: livePhotoFile,
            mountType: mountType,
            name: name,
            note: note,
            ownerId: ownerId,
            ownerDisplayName: ownerDisplayName,
            lock: lock != nil ? true : false,
            lockOwner: lock?.owner,
            lockOwnerEditor: lock?.ownerEditor,
            lockOwnerType: lock?.ownerType.rawValue,
            lockOwnerDisplayName: lock?.ownerDisplayName,
            lockTime: lock?.time,
            lockTimeOut: lock?.timeOut,
            lockToken: lock?.token,
            path: path,
            permissions: permissions,
            quotaUsedBytes: quotaUsedBytes,
            quotaAvailableBytes: quotaAvailableBytes,
            resourceType: resourceType,
            richWorkspace: richWorkspace,
            serverUrl: serverUrl,
            sharePermissionsCollaborationServices: sharePermissionsCollaborationServices,
            sharePermissionsCloudMesh: sharePermissionsCloudMesh,
            shareType: shareType,
            size: size,
            tags: tags,
            uploaded: uploaded,
            trashbinFileName: trashbinFileName,
            trashbinOriginalLocation: trashbinOriginalLocation,
            trashbinDeletionTime: trashbinDeletionTime,
            uploadDate: uploadDate as Date,
            urlBase: urlBase,
            user: user,
            userId: userId
        )
    }
}



extension Array<NKFile> {
    private final actor DirectoryReadConversionActor: Sendable {
        let directoryMetadata: SendableItemMetadata
        var childDirectoriesMetadatas: [SendableItemMetadata] = []
        var metadatas: [SendableItemMetadata] = []

        func convertedMetadatas() -> (
            SendableItemMetadata, [SendableItemMetadata], [SendableItemMetadata]
        ) {
            (directoryMetadata, childDirectoriesMetadatas, metadatas)
        }

        init(target: SendableItemMetadata) {
            self.directoryMetadata = target
        }

        func add(metadata: SendableItemMetadata) {
            metadatas.append(metadata)
            if metadata.directory {
                childDirectoriesMetadatas.append(metadata)
            }
        }
    }

    func toDirectoryReadMetadatas(account: Account) async -> (
        directoryMetadata: SendableItemMetadata,
        childDirectoriesMetadatas: [SendableItemMetadata],
        metadatas: [SendableItemMetadata]
    )? {
        guard var targetDirectoryMetadata = first?.toItemMetadata() else {
            return nil
        }
        let conversionActor = DirectoryReadConversionActor(target: targetDirectoryMetadata)
        if self.count > 1 {
            await self[1...].concurrentChunkedForEach { file in
                await conversionActor.add(metadata: file.toItemMetadata())
            }
        }
        return await conversionActor.convertedMetadatas()
    }
}
