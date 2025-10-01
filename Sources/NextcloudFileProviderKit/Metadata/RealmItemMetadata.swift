//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import RealmSwift

///
/// Realm data model for a file provider item as stored in the extension's own database for metadata.
///
/// > Warning: **Do not pass instances across the boundaries of different concurrency domains because they are not sendable!**
/// Use ``SendableItemMetadata`` as a representation instead.
///
internal class RealmItemMetadata: Object, ItemMetadata {
    @Persisted(primaryKey: true) public var ocId: String
    @Persisted public var account = ""
    @Persisted public var checksums = ""
    @Persisted public var chunkUploadId: String?
    @Persisted public var classFile = ""
    @Persisted public var commentsUnread: Bool = false
    @Persisted public var contentType = ""
    @Persisted public var creationDate = Date()
    @Persisted public var dataFingerprint = ""
    @Persisted public var date = Date()
    @Persisted public var syncTime = Date()
    @Persisted public var deleted = false
    @Persisted public var directory: Bool = false
    @Persisted public var downloadURL = ""
    @Persisted public var e2eEncrypted: Bool = false
    @Persisted public var etag = ""
    @Persisted public var favorite: Bool = false
    @Persisted public var fileId = ""
    @Persisted public var fileName = "" // What the file's real file name is
    @Persisted public var fileNameView = "" // What the user sees (usually same as fileName)
    @Persisted public var hasPreview: Bool = false
    @Persisted public var hidden = false
    @Persisted public var iconName = ""
    @Persisted public var iconUrl = ""
    @Persisted public var isLockFileOfLocalOrigin: Bool = false
    @Persisted public var livePhotoFile: String?
    @Persisted public var mountType = ""
    @Persisted public var name = ""  // for unifiedSearch is the provider.id
    @Persisted public var note = ""
    @Persisted public var ownerId = ""
    @Persisted public var ownerDisplayName = ""
    @Persisted public var lock: Bool = false
    @Persisted public var lockOwner: String?
    @Persisted public var lockOwnerEditor: String?
    @Persisted public var lockOwnerType: Int?
    @Persisted public var lockOwnerDisplayName: String?
    @Persisted public var lockTime: Date? // Time the file was locked
    @Persisted public var lockTimeOut: Date? // Time the file's lock will expire
    @Persisted public var lockToken: String? // Token identifier for token-based locks
    @Persisted public var path = ""
    @Persisted public var permissions = ""
    @Persisted public var quotaUsedBytes: Int64 = 0
    @Persisted public var quotaAvailableBytes: Int64 = 0
    @Persisted public var resourceType = ""
    @Persisted public var richWorkspace: String?
    @Persisted public var serverUrl = ""  // For parent folder! Build remote url by adding fileName
    @Persisted public var session: String?
    @Persisted public var sessionError: String?
    @Persisted public var sessionTaskIdentifier: Int?
    @Persisted public var storedShareType = List<Int>()
    public var shareType: [Int] {
        get { storedShareType.map { $0 } }
        set {
            storedShareType = List<Int>()
            storedShareType.append(objectsIn: newValue)
        }
    }
    @Persisted public var sharePermissionsCollaborationServices: Int = 0
    // TODO: Find a way to compare these two below in remote state check
    @Persisted public var storedSharePermissionsCloudMesh = List<String>()
    public var sharePermissionsCloudMesh: [String] {
        get { storedSharePermissionsCloudMesh.map { $0 } }
        set {
            storedSharePermissionsCloudMesh = List<String>()
            storedSharePermissionsCloudMesh.append(objectsIn: newValue)
        }
    }
    @Persisted public var size: Int64 = 0
    @Persisted public var status: Int = 0
    @Persisted public var storedTags = List<String>()
    public var tags: [String] {
        get { storedTags.map { $0 } }
        set {
            storedTags = List<String>()
            storedTags.append(objectsIn: newValue)
        }
    }
    @Persisted public var downloaded = false
    @Persisted public var uploaded = false
    @Persisted public var keepDownloaded = false
    @Persisted public var visitedDirectory = false
    @Persisted public var trashbinFileName = ""
    @Persisted public var trashbinOriginalLocation = ""
    @Persisted public var trashbinDeletionTime = Date()
    @Persisted public var uploadDate = Date()
    @Persisted public var urlBase = ""
    @Persisted public var user = "" // The user who owns the file (Nextcloud username)
    @Persisted public var userId = "" // The user who owns the file (backend user id)
                                      // (relevant for alt. backends like LDAP)

    public override func isEqual(_ object: Any?) -> Bool {
        if let object = object as? RealmItemMetadata {
            return fileId == object.fileId && account == object.account && path == object.path
                && fileName == object.fileName
        }

        return false
    }

    convenience init(value: any ItemMetadata) {
        self.init()
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
        self.isLockFileOfLocalOrigin = value.isLockFileOfLocalOrigin
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
        self.size = value.size
        self.status = value.status
        self.shareType = value.shareType
        self.tags = value.tags
        self.downloaded = value.downloaded
        self.uploaded = value.uploaded
        self.keepDownloaded = value.keepDownloaded
        self.visitedDirectory = value.visitedDirectory
        self.trashbinFileName = value.trashbinFileName
        self.trashbinOriginalLocation = value.trashbinOriginalLocation
        self.trashbinDeletionTime = value.trashbinDeletionTime
        self.uploadDate = value.uploadDate
        self.urlBase = value.urlBase
        self.user = value.user
        self.userId = value.userId
    }
}
