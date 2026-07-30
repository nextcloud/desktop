//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Value type representation for `RealmItemMetadata`.
///
/// > Warning: Realm objects are inherently unsendable and not thread-safe.
/// **Do not hand them across the boundaries of different concurrency domains!**
/// Ensure that this representation is the only one passed around and always completely abstract Realm to upper layer calling code.
///
public struct SendableItemMetadata: ItemMetadata, Sendable {
    public var ocId: String
    public var account: String
    public var checksums: String
    public var chunkUploadId: String?
    public var classFile: String
    public var commentsUnread: Bool
    public var contentType: String
    public var creationDate: Date
    public var dataFingerprint: String
    public var date: Date
    public var syncTime: Date
    public var deleted: Bool
    public var directory: Bool
    public var downloadURL: String
    public var e2eEncrypted: Bool
    public var etag: String
    public var favorite: Bool
    public var fileId: String
    public var fileName: String
    public var fileNameView: String
    public var hasPreview: Bool
    public var hidden: Bool
    public var iconName: String
    public var iconUrl: String
    public var isLockFileOfLocalOrigin: Bool
    public var mountType: String
    public var name: String
    public var note: String
    public var ownerId: String
    public var ownerDisplayName: String
    public var livePhotoFile: String?
    public var lock: Bool
    public var lockOwner: String?
    public var lockOwnerEditor: String?
    public var lockOwnerType: Int?
    public var lockOwnerDisplayName: String?
    public var lockTime: Date?
    public var lockTimeOut: Date?
    public var lockToken: String?
    public var path: String
    public var permissions: String
    public var quotaUsedBytes: Int64
    public var quotaAvailableBytes: Int64
    public var resourceType: String
    public var richWorkspace: String?
    public var serverUrl: String
    public var session: String?
    public var sessionError: String?
    public var sessionTaskIdentifier: Int?
    public var sharePermissionsCollaborationServices: Int
    public var sharePermissionsCloudMesh: [String]
    public var shareType: [Int]
    public var size: Int64
    public var status: Int
    public var tags: [String]
    public var downloaded: Bool
    public var uploaded: Bool
    public var keepDownloaded: Bool
    public var visitedDirectory: Bool
    public var trashbinFileName: String
    public var trashbinOriginalLocation: String
    public var trashbinDeletionTime: Date
    public var uploadDate: Date
    public var urlBase: String
    public var user: String
    public var userId: String

    public init(
        ocId: String,
        account: String,
        checksums: String = "",
        chunkUploadId: String? = nil,
        classFile: String,
        commentsUnread: Bool = false,
        contentType: String,
        creationDate: Date,
        dataFingerprint: String = "",
        date: Date = Date(),
        syncTime: Date = Date(),
        deleted: Bool = false,
        directory: Bool,
        downloadURL: String = "",
        e2eEncrypted: Bool,
        etag: String,
        favorite: Bool = false,
        fileId: String,
        fileName: String,
        fileNameView: String,
        hasPreview: Bool = false,
        hidden: Bool = false,
        iconName: String = "",
        iconUrl: String = "",
        isLockfileOfLocalOrigin: Bool = false,
        livePhotoFile: String? = nil,
        mountType: String = "",
        name: String = "",
        note: String = "",
        ownerId: String,
        ownerDisplayName: String,
        lock: Bool = false,
        lockOwner: String? = nil,
        lockOwnerEditor: String? = nil,
        lockOwnerType: Int? = nil,
        lockOwnerDisplayName: String? = nil,
        lockTime: Date? = nil,
        lockTimeOut: Date? = nil,
        lockToken: String? = nil,
        path: String,
        permissions: String = "RGDNVW",
        quotaUsedBytes: Int64 = 0,
        quotaAvailableBytes: Int64 = 0,
        resourceType: String = "",
        richWorkspace: String? = nil,
        serverUrl: String,
        session: String? = nil,
        sessionError: String? = nil,
        sessionTaskIdentifier: Int? = nil,
        sharePermissionsCollaborationServices: Int = 0,
        sharePermissionsCloudMesh: [String] = [],
        shareType: [Int] = [],
        size: Int64,
        status: Int = 0,
        tags: [String] = [],
        downloaded: Bool = false,
        uploaded: Bool = false,
        keepDownloaded: Bool = false,
        visitedDirectory: Bool = false,
        trashbinFileName: String = "",
        trashbinOriginalLocation: String = "",
        trashbinDeletionTime: Date = Date(),
        uploadDate: Date = Date(),
        urlBase: String,
        user: String,
        userId: String
    ) {
        self.ocId = ocId
        self.account = account
        self.checksums = checksums
        self.chunkUploadId = chunkUploadId
        self.classFile = classFile
        self.commentsUnread = commentsUnread
        self.contentType = contentType
        self.creationDate = creationDate
        self.dataFingerprint = dataFingerprint
        self.date = date
        self.syncTime = syncTime
        self.deleted = deleted
        self.directory = directory
        self.downloadURL = downloadURL
        self.e2eEncrypted = e2eEncrypted
        self.etag = etag
        self.favorite = favorite
        self.fileId = fileId
        self.fileName = fileName
        self.fileNameView = fileNameView
        self.hasPreview = hasPreview
        self.hidden = hidden
        self.iconName = iconName
        self.iconUrl = iconUrl
        isLockFileOfLocalOrigin = isLockfileOfLocalOrigin
        self.livePhotoFile = livePhotoFile
        self.mountType = mountType
        self.name = name
        self.note = note
        self.ownerId = ownerId
        self.ownerDisplayName = ownerDisplayName
        self.lock = lock
        self.lockOwner = lockOwner
        self.lockOwnerEditor = lockOwnerEditor
        self.lockOwnerType = lockOwnerType
        self.lockOwnerDisplayName = lockOwnerDisplayName
        self.lockTime = lockTime
        self.lockTimeOut = lockTimeOut
        self.lockToken = lockToken
        self.path = path
        self.permissions = permissions
        self.quotaUsedBytes = quotaUsedBytes
        self.quotaAvailableBytes = quotaAvailableBytes
        self.resourceType = resourceType
        self.richWorkspace = richWorkspace
        self.serverUrl = serverUrl
        self.session = session
        self.sessionError = sessionError
        self.sessionTaskIdentifier = sessionTaskIdentifier
        self.sharePermissionsCollaborationServices = sharePermissionsCollaborationServices
        self.sharePermissionsCloudMesh = sharePermissionsCloudMesh
        self.shareType = shareType
        self.size = size
        self.status = status
        self.tags = tags
        self.downloaded = downloaded
        self.uploaded = uploaded
        self.keepDownloaded = keepDownloaded
        self.visitedDirectory = visitedDirectory
        self.trashbinFileName = trashbinFileName
        self.trashbinOriginalLocation = trashbinOriginalLocation
        self.trashbinDeletionTime = trashbinDeletionTime
        self.uploadDate = uploadDate
        self.urlBase = urlBase
        self.user = user
        self.userId = userId
    }

    init(value: any ItemMetadata) {
        ocId = value.ocId
        account = value.account
        checksums = value.checksums
        chunkUploadId = value.chunkUploadId
        classFile = value.classFile
        commentsUnread = value.commentsUnread
        contentType = value.contentType
        creationDate = value.creationDate
        dataFingerprint = value.dataFingerprint
        date = value.date
        syncTime = value.syncTime
        deleted = value.deleted
        directory = value.directory
        downloadURL = value.downloadURL
        e2eEncrypted = value.e2eEncrypted
        etag = value.etag
        favorite = value.favorite
        fileId = value.fileId
        fileName = value.fileName
        fileNameView = value.fileNameView
        hasPreview = value.hasPreview
        hidden = value.hidden
        iconName = value.iconName
        iconUrl = value.iconUrl
        isLockFileOfLocalOrigin = value.isLockFileOfLocalOrigin
        livePhotoFile = value.livePhotoFile
        mountType = value.mountType
        name = value.name
        note = value.note
        ownerId = value.ownerId
        ownerDisplayName = value.ownerDisplayName
        lock = value.lock
        lockOwner = value.lockOwner
        lockOwnerEditor = value.lockOwnerEditor
        lockOwnerType = value.lockOwnerType
        lockOwnerDisplayName = value.lockOwnerDisplayName
        lockTime = value.lockTime
        lockTimeOut = value.lockTimeOut
        lockToken = value.lockToken
        path = value.path
        permissions = value.permissions
        quotaUsedBytes = value.quotaUsedBytes
        quotaAvailableBytes = value.quotaAvailableBytes
        resourceType = value.resourceType
        richWorkspace = value.richWorkspace
        serverUrl = value.serverUrl
        session = value.session
        sessionError = value.sessionError
        sessionTaskIdentifier = value.sessionTaskIdentifier
        sharePermissionsCollaborationServices = value.sharePermissionsCollaborationServices
        sharePermissionsCloudMesh = value.sharePermissionsCloudMesh
        shareType = value.shareType
        size = value.size
        status = value.status
        downloaded = value.downloaded
        uploaded = value.uploaded
        keepDownloaded = value.keepDownloaded
        visitedDirectory = value.visitedDirectory
        tags = value.tags
        trashbinFileName = value.trashbinFileName
        trashbinOriginalLocation = value.trashbinOriginalLocation
        trashbinDeletionTime = value.trashbinDeletionTime
        uploadDate = value.uploadDate
        urlBase = value.urlBase
        user = value.user
        userId = value.userId
    }
}
