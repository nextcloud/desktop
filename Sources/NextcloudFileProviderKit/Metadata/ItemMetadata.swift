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
import RealmSwift

public enum Status: Int {
    case downloadError = -4
    case downloading = -3
    case inDownload = -2

    case normal = 0

    case inUpload = 2
    case uploading = 3
    case uploadError = 4
}

public enum SharePermissions: Int {
    case readShare = 1
    case updateShare = 2
    case createShare = 4
    case deleteShare = 8
    case shareShare = 16

    case maxFileShare = 19
    case maxFolderShare = 31
}

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
    var mountType: String { get set }
    var name: String { get set }  // for unifiedSearch is the provider.id
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
    var path: String { get set }
    var permissions: String { get set }
    var shareType: [Int] { get set }
    var quotaUsedBytes: Int64 { get set }
    var quotaAvailableBytes: Int64 { get set }
    var resourceType: String { get set }
    var richWorkspace: String? { get set }
    var serverUrl: String { get set }  // For parent folder! Build remote url by adding fileName
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

        let urlBase = urlBase.urlEncoded!
        // Leave the leading slash in webdavUrl
        let webdavUrl = urlBase + Account.webDavFilesUrlSuffix + user
        let serverFileRelativeUrl =
            serverUrl.replacingOccurrences(of: webdavUrl, with: "") + "/" + fileName

        let urlString =
            "\(urlBase)/index.php/core/preview.png?file=\(serverFileRelativeUrl)&x=\(size.width)&y=\(size.height)&a=1&mode=cover"
        return URL(string: urlString)
    }
}

public class RealmItemMetadata: Object, ItemMetadata {
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
    public var sharePermissionsCloudMesh = [String]()
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
        self.trashbinFileName = value.trashbinFileName
        self.trashbinOriginalLocation = value.trashbinOriginalLocation
        self.trashbinDeletionTime = value.trashbinDeletionTime
        self.uploadDate = value.uploadDate
        self.urlBase = value.urlBase
        self.user = value.user
        self.userId = value.userId
    }
}

/// Realm objects are inherently unsendable and not thread-safe **IF THEY ARE MANAGED.**
/// Marking our ItemMetadata as an unchecked Sendable is a naughty thing to do. So make sure to check
/// for ItemMetadata objects to be unmanaged before doing anything crossing actor boundaries.
/// Also make sure this is the only type that is returned to other code that is unaware of Realm.
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
        directory: Bool,
        downloadURL: String = "",
        e2eEncrypted: Bool,
        etag: String,
        favorite: Bool = false,
        fileId: String,
        fileName: String,
        fileNameView: String,
        hasPreview: Bool,
        hidden: Bool = false,
        iconName: String,
        iconUrl: String = "",
        livePhotoFile: String? = nil,
        mountType: String,
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
