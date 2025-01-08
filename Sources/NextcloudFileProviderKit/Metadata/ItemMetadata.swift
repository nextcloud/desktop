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

public class ItemMetadata: Object {
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

    @Persisted(primaryKey: true) public var ocId: String
    @Persisted public var account = ""
    @Persisted public var assetLocalIdentifier = ""
    @Persisted public var checksums = ""
    @Persisted public var chunkUploadId: String = ""
    @Persisted public var classFile = ""
    @Persisted public var commentsUnread: Bool = false
    @Persisted public var contentType = ""
    @Persisted public var creationDate = Date()
    @Persisted public var dataFingerprint = ""
    @Persisted public var date = Date()
    @Persisted public var directory: Bool = false
    @Persisted public var deleteAssetLocalIdentifier: Bool = false
    @Persisted public var downloadURL = ""
    @Persisted public var e2eEncrypted: Bool = false
    @Persisted public var edited: Bool = false
    @Persisted public var etag = ""
    @Persisted public var etagResource = ""
    @Persisted public var favorite: Bool = false
    @Persisted public var fileId = ""
    @Persisted public var fileName = "" // What the file's real file name is
    @Persisted public var fileNameView = "" // What the user sees (usually same as fileName)
    @Persisted public var hasPreview: Bool = false
    @Persisted public var iconName = ""
    @Persisted public var iconUrl = ""
    @Persisted public var isExtractFile: Bool = false
    @Persisted public var livePhoto: Bool = false
    @Persisted public var mountType = ""
    @Persisted public var name = ""  // for unifiedSearch is the provider.id
    @Persisted public var note = ""
    @Persisted public var ownerId = ""
    @Persisted public var ownerDisplayName = ""
    @Persisted public var lock = false
    @Persisted public var lockOwner = ""
    @Persisted public var lockOwnerEditor = ""
    @Persisted public var lockOwnerType = 0
    @Persisted public var lockOwnerDisplayName = ""
    @Persisted public var lockTime: Date? // Time the file was locked
    @Persisted public var lockTimeOut: Date? // Time the file's lock will expire
    @Persisted public var path = ""
    @Persisted public var permissions = ""
    @Persisted public var quotaUsedBytes: Int64 = 0
    @Persisted public var quotaAvailableBytes: Int64 = 0
    @Persisted public var resourceType = ""
    @Persisted public var richWorkspace: String?
    @Persisted public var serverUrl = ""  // For parent folder! Build remote url by adding fileName
    @Persisted public var session = ""
    @Persisted public var sessionError = ""
    @Persisted public var sessionSelector = ""
    @Persisted public var sessionTaskIdentifier: Int = 0
    @Persisted public var sharePermissionsCollaborationServices: Int = 0
    // TODO: Find a way to compare these two below in remote state check
    public let sharePermissionsCloudMesh = List<String>()
    public let shareType = List<Int>()
    @Persisted public var size: Int64 = 0
    @Persisted public var status: Int = 0
    @Persisted public var downloaded = false
    @Persisted public var uploaded = false
    @Persisted public var subline: String?
    @Persisted public var trashbinFileName = ""
    @Persisted public var trashbinOriginalLocation = ""
    @Persisted public var trashbinDeletionTime = Date()
    @Persisted public var uploadDate = Date()
    @Persisted public var url = ""
    @Persisted public var urlBase = ""
    @Persisted public var user = "" // The user who owns the file (Nextcloud username)
    @Persisted public var userId = "" // The user who owns the file (backend user id)
                                      // (relevant for alt. backends like LDAP)

    public var fileExtension: String {
        (fileNameView as NSString).pathExtension
    }

    public var fileNoExtension: String {
        (fileNameView as NSString).deletingPathExtension
    }

    public var isRenameable: Bool {
        lock
    }

    public var isPrintable: Bool {
        if isDocumentViewableOnly {
            return false
        }
        if ["application/pdf", "com.adobe.pdf"].contains(contentType)
            || contentType.hasPrefix("text/")
            || classFile == NKCommon.TypeClassFile.image.rawValue
        {
            return true
        }
        return false
    }

    public var isDocumentViewableOnly: Bool {
        sharePermissionsCollaborationServices == SharePermissions.readShare.rawValue
            && classFile == NKCommon.TypeClassFile.document.rawValue
    }

    public var isCopyableInPasteboard: Bool {
        !isDocumentViewableOnly && !directory
    }

    public var isModifiableWithQuickLook: Bool {
        if directory || isDocumentViewableOnly {
            return false
        }
        return contentType == "com.adobe.pdf" || contentType == "application/pdf"
            || classFile == NKCommon.TypeClassFile.image.rawValue
    }

    public var isSettableOnOffline: Bool {
        session.isEmpty && !isDocumentViewableOnly
    }

    public var canOpenIn: Bool {
        session.isEmpty && !isDocumentViewableOnly && !directory
    }

    public var isDownloadUpload: Bool {
        status == Status.inDownload.rawValue || status == Status.downloading.rawValue
            || status == Status.inUpload.rawValue || status == Status.uploading.rawValue
    }

    public var isDownload: Bool {
        status == Status.inDownload.rawValue || status == Status.downloading.rawValue
    }

    public var isUpload: Bool {
        status == Status.inUpload.rawValue || status == Status.uploading.rawValue
    }

    public var isTrashed: Bool {
        serverUrl.hasPrefix(urlBase + Account.webDavTrashUrlSuffix + userId + "/trash")
    }

    public override func isEqual(_ object: Any?) -> Bool {
        if let object = object as? ItemMetadata {
            return fileId == object.fileId && account == object.account && path == object.path
                && fileName == object.fileName
        }

        return false
    }

    public func isInSameDatabaseStoreableRemoteState(_ comparingMetadata: ItemMetadata)
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
    public func canUnlock(as user: String) -> Bool {
        !lock || (lockOwner == user && lockOwnerType == 0)
    }

    public func thumbnailUrl(size: CGSize) -> URL? {
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

    public func apply(fileName: String) {
        self.fileName = fileName
        fileNameView = fileName
        name = fileName
    }

    public func apply(account: Account) {
        self.account = account.ncKitAccount
        user = account.username
        userId = account.id
        urlBase = account.serverUrl
    }
}
