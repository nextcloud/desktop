//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// Used as a semantic mapping for ``ItemMetadata/status``.
///
public enum Status: Int {
    case downloadError = -4
    case downloading = -3
    case inDownload = -2

    case normal = 0

    case inUpload = 2
    case uploading = 3
    case uploadError = 4
}

///
/// Requirements for the data model implementations of file provider items.
///
public protocol ItemMetadata: Equatable {
    var ocId: String { get set }
    var account: String { get set }
    var checksums: String { get set }
    var chunkUploadId: String? { get set }
    var classFile: String { get set }
    var commentsUnread: Bool { get set }
    var contentType: String { get set }
    var creationDate: Date { get set }
    var dataFingerprint: String { get set }
    var date: Date { get set }
    var syncTime: Date { get set }
    var deleted: Bool { get set }
    var directory: Bool { get set }
    var downloadURL: String { get set }
    var e2eEncrypted: Bool { get set }
    var etag: String { get set }
    var favorite: Bool { get set }
    var fileId: String { get set }
    var fileName: String { get set } // What the file's real file name is
    var fileNameView: String { get set } // What the user sees (usually same as fileName)
    var hasPreview: Bool { get set }
    var hidden: Bool { get set }
    var iconName: String { get set }
    var iconUrl: String { get set }

    ///
    /// This is a lock file which was created on the local device and not introduced through synchronization with the server.
    ///
    var isLockFileOfLocalOrigin: Bool { get set }

    var mountType: String { get set }
    var name: String { get set } // for unifiedSearch is the provider.id
    var note: String { get set }
    var ownerId: String { get set }
    var ownerDisplayName: String { get set }
    var livePhotoFile: String? { get set }
    var lock: Bool { get set }
    var lockOwner: String? { get set }
    var lockOwnerEditor: String? { get set }
    var lockOwnerType: Int? { get set }
    var lockOwnerDisplayName: String? { get set }
    var lockTime: Date? { get set } // Time the file was locked
    var lockTimeOut: Date? { get set } // Time the file's lock will expire
    var lockToken: String? { get set }
    var path: String { get set }
    var permissions: String { get set }
    var shareType: [Int] { get set }
    var quotaUsedBytes: Int64 { get set }
    var quotaAvailableBytes: Int64 { get set }
    var resourceType: String { get set }
    var richWorkspace: String? { get set }
    var serverUrl: String { get set } // For parent folder! Retrieve the full remote url via .remotePath()
    var session: String? { get set }
    var sessionError: String? { get set }
    var sessionTaskIdentifier: Int? { get set }
    var sharePermissionsCollaborationServices: Int { get set }
    // TODO: Find a way to compare these two below in remote state check
    var sharePermissionsCloudMesh: [String] { get set }
    var size: Int64 { get set }
    var status: Int { get set }
    var tags: [String] { get set }
    var downloaded: Bool { get set }
    var uploaded: Bool { get set }
    var keepDownloaded: Bool { get set }
    var visitedDirectory: Bool { get set }
    var trashbinFileName: String { get set }
    var trashbinOriginalLocation: String { get set }
    var trashbinDeletionTime: Date { get set }
    var uploadDate: Date { get set }
    var urlBase: String { get set }
    var user: String { get set } // The user who owns the file (Nextcloud username)
    var userId: String { get set } // The user who owns the file (backend user id)
    // (relevant for alt. backends like LDAP)
}

public extension ItemMetadata {
    var livePhoto: Bool {
        livePhotoFile != nil && livePhotoFile?.isEmpty == false
    }

    var isDownloadUpload: Bool {
        status == Status.inDownload.rawValue || status == Status.downloading.rawValue
            || status == Status.inUpload.rawValue || status == Status.uploading.rawValue
    }

    var isDownload: Bool {
        status == Status.inDownload.rawValue || status == Status.downloading.rawValue
    }

    var isUpload: Bool {
        status == Status.inUpload.rawValue || status == Status.uploading.rawValue
    }

    var isTrashed: Bool {
        serverUrl.hasPrefix(urlBase + Account.webDavTrashUrlSuffix + userId + "/trash")
    }

    mutating func apply(fileName: String) {
        self.fileName = fileName
        fileNameView = fileName
        name = fileName
    }

    mutating func apply(account: Account) {
        self.account = account.ncKitAccount
        user = account.username
        userId = account.id
        urlBase = account.serverUrl
    }

    func isInSameDatabaseStoreableRemoteState(_ comparingMetadata: any ItemMetadata)
        -> Bool
    {
        comparingMetadata.etag == etag
            && comparingMetadata.fileNameView == fileNameView
            && comparingMetadata.date == date
            && comparingMetadata.permissions == permissions
            && comparingMetadata.hasPreview == hasPreview
            && comparingMetadata.note == note
            && comparingMetadata.lock == lock
            && comparingMetadata.sharePermissionsCollaborationServices
            == sharePermissionsCollaborationServices
            && comparingMetadata.favorite == favorite
    }

    /// Returns false if the user is lokced out of the file. I.e. The file is locked but by someone else
    func canUnlock(as user: String) -> Bool {
        !lock || (lockOwner == user && lockOwnerType == 0)
    }

    func thumbnailUrl(size: CGSize) -> URL? {
        guard hasPreview else {
            return nil
        }

        return URL(string: urlBase.urlEncoded ?? "")?
            .appending(components: "index.php", "core", "preview")
            .appending(queryItems: [
                .init(name: "fileId", value: fileId),
                .init(name: "x", value: "\(size.width)"),
                .init(name: "y", value: "\(size.height)"),
                .init(name: "a", value: "true")
            ])
    }

    func remotePath() -> String {
        if ocId == NSFileProviderItemIdentifier.rootContainer.rawValue {
            // For the root container the fileName is defined by NextcloudKit.shared.nkCommonInstance.rootFileName.
            // --> appending the fileName to that is not correct, as it most likely won't exist.
            return serverUrl
        }

        return "\(serverUrl)/\(fileName)"
    }
}
