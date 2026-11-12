//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

extension NKFile {
    /// Use this response's server base URL and backend user ID to replace the matching files or trash
    /// root in `url` with `files:` or `trash:`. For example,
    /// `https://cloud.example.com/remote.php/dav/files/alice/Team/Docs` becomes `files:/Team/Docs`.
    /// Returns `nil` if neither root matches.
    private func davRelativePath(forResponseURL url: String) -> String? {
        Account.davRelativePath(
            forURL: url,
            accountBaseURL: urlBase,
            backendUserID: userId,
            acceptEquivalentResponseWebroot: true
        )
    }

    private var representsAccountFilesRoot: Bool {
        Account.isFilesRootAddress(
            parentURL: serverUrl,
            fileName: fileName,
            accountBaseURL: urlBase,
            backendUserID: userId,
            acceptEquivalentResponseWebroot: true
        )
    }

    func matchesRequestedDirectoryURL(_ requestedURL: String) -> Bool {
        let requestedPath = Account.davRelativePath(forURL: requestedURL, accountBaseURL: urlBase, backendUserID: userId)
        if representsAccountFilesRoot {
            return requestedPath.map(Account.isStoredFilesRootLocation) ?? false
        }

        guard directory else { return false }

        let responseURL = serverUrl + "/" + fileName
        if let requestedPath, let responsePath = davRelativePath(forResponseURL: responseURL) {
            return responsePath == requestedPath
        }
        return responseURL.utf8.elementsEqual(requestedURL.utf8)
    }

    func matchesResponseItemURL(_ expectedURL: String) -> Bool {
        var responseItemURL = serverUrl + "/" + fileName
        if responseItemURL.last == "/" { // This is likely the root container, as it has no filename
            responseItemURL.removeLast()
        }
        return responseItemURL == expectedURL
    }

    func toItemMetadata(uploaded: Bool = true) -> SendableItemMetadata {
        let creationDate = creationDate ?? date
        let uploadDate = uploadDate ?? date

        let classFile = (contentType == "text/markdown" || contentType == "text/x-markdown") && classFile == NKTypeClassFile.unknow.rawValue
            ? NKTypeClassFile.document.rawValue
            : classFile

        // Support for finding the correct filename for e2ee files should go here

        let isFilesRoot = representsAccountFilesRoot
        // Keep the existing identifier mapping for the SDK's synthetic root.
        let usesSDKFilesRootPlaceholder = isFilesRoot && fileName == NextcloudKit.shared.nkCommonInstance.rootFileName
        let ocId = usesSDKFilesRootPlaceholder ? NSFileProviderItemIdentifier.rootContainer.rawValue : ocId
        let fileName = isFilesRoot ? SendableItemMetadata.filesRootPlaceholderName : fileName
        let directory = isFilesRoot || directory
        let relativeParentPath = isFilesRoot ? Account.storedFilesRootLocation : davRelativePath(forResponseURL: serverUrl)
        let absoluteParentURL = relativeParentPath.flatMap {
            Account.absoluteURL(fromDAVRelativePath: $0, accountBaseURL: urlBase, backendUserID: userId)
        } ?? serverUrl

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
            permissions: permissions,
            quotaUsedBytes: quotaUsedBytes,
            quotaAvailableBytes: quotaAvailableBytes,
            resourceType: resourceType,
            richWorkspace: richWorkspace,
            serverUrl: absoluteParentURL,
            sharePermissionsCollaborationServices: sharePermissionsCollaborationServices,
            sharePermissionsCloudMesh: sharePermissionsCloudMesh,
            shareType: shareType,
            size: size,
            tags: tags.map(\.name),
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
    /// Identify the requested directory in the DAV response and convert it and the remaining response entries to metadata.
    func toSendableDirectoryMetadata(requestedDirectoryURL: String) async -> (root: SendableItemMetadata, directories: [SendableItemMetadata], files: [SendableItemMetadata])? {
        guard let root = first(where: { $0.matchesRequestedDirectoryURL(requestedDirectoryURL) })?.toItemMetadata() else {
            return nil
        }

        let container = DirectoryMetadataContainer(for: root)

        if count > 1 {
            await concurrentChunkedForEach { file in
                guard file.matchesRequestedDirectoryURL(requestedDirectoryURL) == false else {
                    return
                }

                await container.add(file.toItemMetadata())
            }
        }

        return await container.content()
    }
}
