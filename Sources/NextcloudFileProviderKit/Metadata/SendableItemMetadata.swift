//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

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
        self.ocId = value.ocId
        self.account = value.account
        self.checksums = value.checksums
        self.chunkUploadId = value.chunkUploadId
        self.classFile = value.classFile
        self.commentsUnread = value.commentsUnread
        self.contentType = value.contentType
        self.creationDate = value.creationDate
        self.dataFingerprint = value.dataFingerprint
        self.date = value.date
        self.syncTime = value.syncTime
        self.deleted = value.deleted
        self.directory = value.directory
        self.downloadURL = value.downloadURL
        self.e2eEncrypted = value.e2eEncrypted
        self.etag = value.etag
        self.favorite = value.favorite
        self.fileId = value.fileId
        self.fileName = value.fileName
        self.fileNameView = value.fileNameView
        self.hasPreview = value.hasPreview
        self.hidden = value.hidden
        self.iconName = value.iconName
        self.iconUrl = value.iconUrl
        self.livePhotoFile = value.livePhotoFile
        self.mountType = value.mountType
        self.name = value.name
        self.note = value.note
        self.ownerId = value.ownerId
        self.ownerDisplayName = value.ownerDisplayName
        self.lock = value.lock
        self.lockOwner = value.lockOwner
        self.lockOwnerEditor = value.lockOwnerEditor
        self.lockOwnerType = value.lockOwnerType
        self.lockOwnerDisplayName = value.lockOwnerDisplayName
        self.lockTime = value.lockTime
        self.lockTimeOut = value.lockTimeOut
        self.lockToken = value.lockToken
        self.path = value.path
        self.permissions = value.permissions
        self.quotaUsedBytes = value.quotaUsedBytes
        self.quotaAvailableBytes = value.quotaAvailableBytes
        self.resourceType = value.resourceType
        self.richWorkspace = value.richWorkspace
        self.serverUrl = value.serverUrl
        self.session = value.session
        self.sessionError = value.sessionError
        self.sessionTaskIdentifier = value.sessionTaskIdentifier
        self.sharePermissionsCollaborationServices = value.sharePermissionsCollaborationServices
        self.sharePermissionsCloudMesh = value.sharePermissionsCloudMesh
        self.shareType = value.shareType
        self.size = value.size
        self.status = value.status
        self.downloaded = value.downloaded
        self.uploaded = value.uploaded
        self.keepDownloaded = value.keepDownloaded
        self.visitedDirectory = value.visitedDirectory
        self.tags = value.tags
        self.trashbinFileName = value.trashbinFileName
        self.trashbinOriginalLocation = value.trashbinOriginalLocation
        self.trashbinDeletionTime = value.trashbinDeletionTime
        self.uploadDate = value.uploadDate
        self.urlBase = value.urlBase
        self.user = value.user
        self.userId = value.userId
    }
}

