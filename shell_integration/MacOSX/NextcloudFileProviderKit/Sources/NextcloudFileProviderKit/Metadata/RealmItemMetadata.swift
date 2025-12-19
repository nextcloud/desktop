//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import RealmSwift

///
/// Realm data model for a file provider item as stored in the extension's own database for metadata.
///
/// > Warning: **Do not pass instances across the boundaries of different concurrency domains because they are not sendable!**
/// Use ``SendableItemMetadata`` as a representation instead.
///
class RealmItemMetadata: Object, ItemMetadata {
    @Persisted(primaryKey: true) var ocId: String
    @Persisted var account = ""
    @Persisted var checksums = ""
    @Persisted var chunkUploadId: String?
    @Persisted var classFile = ""
    @Persisted var commentsUnread: Bool = false
    @Persisted var contentType = ""
    @Persisted var creationDate = Date()
    @Persisted var dataFingerprint = ""
    @Persisted var date = Date()
    @Persisted var syncTime = Date()
    @Persisted var deleted = false
    @Persisted var directory: Bool = false
    @Persisted var downloadURL = ""
    @Persisted var e2eEncrypted: Bool = false
    @Persisted var etag = ""
    @Persisted var favorite: Bool = false
    @Persisted var fileId = ""
    @Persisted var fileName = "" // What the file's real file name is
    @Persisted var fileNameView = "" // What the user sees (usually same as fileName)
    @Persisted var hasPreview: Bool = false
    @Persisted var hidden = false
    @Persisted var iconName = ""
    @Persisted var iconUrl = ""
    @Persisted var isLockFileOfLocalOrigin: Bool = false
    @Persisted var livePhotoFile: String?
    @Persisted var mountType = ""
    @Persisted var name = "" // for unifiedSearch is the provider.id
    @Persisted var note = ""
    @Persisted var ownerId = ""
    @Persisted var ownerDisplayName = ""
    @Persisted var lock: Bool = false
    @Persisted var lockOwner: String?
    @Persisted var lockOwnerEditor: String?
    @Persisted var lockOwnerType: Int?
    @Persisted var lockOwnerDisplayName: String?
    @Persisted var lockTime: Date? // Time the file was locked
    @Persisted var lockTimeOut: Date? // Time the file's lock will expire
    @Persisted var lockToken: String? // Token identifier for token-based locks
    @Persisted var path = ""
    @Persisted var permissions = ""
    @Persisted var quotaUsedBytes: Int64 = 0
    @Persisted var quotaAvailableBytes: Int64 = 0
    @Persisted var resourceType = ""
    @Persisted var richWorkspace: String?
    @Persisted var serverUrl = "" // For parent folder! Build remote url by adding fileName
    @Persisted var session: String?
    @Persisted var sessionError: String?
    @Persisted var sessionTaskIdentifier: Int?
    @Persisted var storedShareType = List<Int>()
    var shareType: [Int] {
        get { storedShareType.map(\.self) }
        set {
            storedShareType = List<Int>()
            storedShareType.append(objectsIn: newValue)
        }
    }

    @Persisted var sharePermissionsCollaborationServices: Int = 0
    // TODO: Find a way to compare these two below in remote state check
    @Persisted var storedSharePermissionsCloudMesh = List<String>()
    var sharePermissionsCloudMesh: [String] {
        get { storedSharePermissionsCloudMesh.map(\.self) }
        set {
            storedSharePermissionsCloudMesh = List<String>()
            storedSharePermissionsCloudMesh.append(objectsIn: newValue)
        }
    }

    @Persisted var size: Int64 = 0
    @Persisted var status: Int = 0
    @Persisted var storedTags = List<String>()
    var tags: [String] {
        get { storedTags.map(\.self) }
        set {
            storedTags = List<String>()
            storedTags.append(objectsIn: newValue)
        }
    }

    @Persisted var downloaded = false
    @Persisted var uploaded = false
    @Persisted var keepDownloaded = false
    @Persisted var visitedDirectory = false
    @Persisted var trashbinFileName = ""
    @Persisted var trashbinOriginalLocation = ""
    @Persisted var trashbinDeletionTime = Date()
    @Persisted var uploadDate = Date()
    @Persisted var urlBase = ""
    @Persisted var user = "" // The user who owns the file (Nextcloud username)
    @Persisted var userId = "" // The user who owns the file (backend user id)
    // (relevant for alt. backends like LDAP)

    override func isEqual(_ object: Any?) -> Bool {
        if let object = object as? RealmItemMetadata {
            return fileId == object.fileId && account == object.account && path == object.path
                && fileName == object.fileName
        }

        return false
    }

    convenience init(value: any ItemMetadata) {
        self.init()
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
        size = value.size
        status = value.status
        shareType = value.shareType
        tags = value.tags
        downloaded = value.downloaded
        uploaded = value.uploaded
        keepDownloaded = value.keepDownloaded
        visitedDirectory = value.visitedDirectory
        trashbinFileName = value.trashbinFileName
        trashbinOriginalLocation = value.trashbinOriginalLocation
        trashbinDeletionTime = value.trashbinDeletionTime
        uploadDate = value.uploadDate
        urlBase = value.urlBase
        user = value.user
        userId = value.userId
    }
}
