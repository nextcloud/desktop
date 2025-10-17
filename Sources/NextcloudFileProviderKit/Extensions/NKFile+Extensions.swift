//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
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

        let classFile = (contentType == "text/markdown" || contentType == "text/x-markdown") && classFile == NKTypeClassFile.unknow.rawValue
            ? NKTypeClassFile.document.rawValue
            : classFile

        // Support for finding the correct filename for e2ee files should go here

        // Don't ask me why, NextcloudKit renames and moves the root folder details
        // Also don't ask me why, but, NextcloudKit marks the NKFile for this as not a directory
        let rootServerUrl = urlBase + Account.webDavFilesUrlSuffix + userId
        let rootRequiresFixup = serverUrl == rootServerUrl && fileName == NextcloudKit.shared.nkCommonInstance.rootFileName
        let ocId = rootRequiresFixup ? NSFileProviderItemIdentifier.rootContainer.rawValue : ocId
        let directory = rootRequiresFixup ? true : directory
        let serverUrl = rootRequiresFixup ? rootServerUrl : serverUrl
        let fileName = rootRequiresFixup ? NextcloudKit.shared.nkCommonInstance.rootFileName : fileName

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
            lock: lock,
            lockOwner: lockOwner,
            lockOwnerEditor: lockOwnerEditor,
            lockOwnerType: lockOwnerType,
            lockOwnerDisplayName: lockOwnerDisplayName,
            lockTime: lockTime,
            lockTimeOut: lockTimeOut,
            lockToken: nil, // This is not available at this point and must be fetched from the local persistence later.
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

///
/// Data container intended for use in combination with `concurrentChunkedForEach` to safely and concurrently convert a lot of metadata objects.
///
private final actor DirectoryMetadataContainer: Sendable {
    let root: SendableItemMetadata
    var directories: [SendableItemMetadata] = []
    var files: [SendableItemMetadata] = []

    init(for root: SendableItemMetadata) {
        self.root = root
    }

    ///
    /// Insert a new item into the container.
    ///
    func add(_ item: SendableItemMetadata) {
        files.append(item)

        if item.directory {
            directories.append(item)
        }
    }

    ///
    /// Return a tuple of the total current content.
    ///
    func content() -> (SendableItemMetadata, [SendableItemMetadata], [SendableItemMetadata]) {
        (root, directories, files)
    }
}

extension [NKFile] {
    ///
    /// Determine whether the given `NKFile` is the metadata object for the read remote directory.
    ///
    func isDirectoryToRead(_ file: NKFile, directoryToRead: String) -> Bool {
        if file.serverUrl == directoryToRead, file.fileName == NextcloudKit.shared.nkCommonInstance.rootFileName {
            return true
        }

        if file.directory, directoryToRead == "\(file.serverUrl)/\(file.fileName)" {
            return true
        }

        return false
    }

    ///
    /// Convert an array of `NKFile` to `SendableItemMetadata`.
    ///
    /// - Parameters:
    ///     - account: The account which the metadata belongs to.
    ///     - directoryToRead: The root path of the directory which this metadata comes from. This is required to distinguish the correct item for the metadata of the read directory itself from its children.
    ///
    /// - Returns: A tuple consisting of the metadata for the read directory itself (`root`), any child directories (`directories`) and separately any directly containted files (`files`).
    ///
    func toSendableDirectoryMetadata(account _: Account, directoryToRead: String) async -> (root: SendableItemMetadata, directories: [SendableItemMetadata], files: [SendableItemMetadata])? {
        guard let root = first(where: { isDirectoryToRead($0, directoryToRead: directoryToRead) })?.toItemMetadata() else {
            return nil
        }

        let container = DirectoryMetadataContainer(for: root)

        if count > 1 {
            await concurrentChunkedForEach { file in
                guard isDirectoryToRead(file, directoryToRead: directoryToRead) == false else {
                    return
                }

                await container.add(file.toItemMetadata())
            }
        }

        return await container.content()
    }
}
