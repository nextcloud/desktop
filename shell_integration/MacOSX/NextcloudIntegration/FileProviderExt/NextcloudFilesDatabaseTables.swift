/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import RealmSwift

class NextcloudItemMetadataTable: Object {
    enum Status: Int {
        case downloadError = -4
        case downloading = -3
        case inDownload = -2
        case waitDownload = -1

        case normal = 0

        case waitUpload = 1
        case inUpload = 2
        case uploading = 3
        case uploadError = 4
    }

    override func isEqual(_ object: Any?) -> Bool {
        if let object = object as? NextcloudItemMetadataTable {
            return self.fileId == object.fileId &&
                   self.account == object.account &&
                   self.path == object.path &&
                   self.fileName == object.fileName
        }

        return false
    }

    func isInSameRemoteState(_ comparingMetadata: NextcloudItemMetadataTable) -> Bool {
        return comparingMetadata.etag == self.etag &&
            comparingMetadata.fileNameView == self.fileNameView &&
            comparingMetadata.date == self.date &&
            comparingMetadata.permissions == self.permissions &&
            comparingMetadata.hasPreview == self.hasPreview &&
            comparingMetadata.note == self.note &&
            comparingMetadata.lock == self.lock &&
            comparingMetadata.shareType == self.shareType &&
            comparingMetadata.sharePermissionsCloudMesh == self.sharePermissionsCloudMesh &&
            comparingMetadata.sharePermissionsCollaborationServices == self.sharePermissionsCollaborationServices &&
            comparingMetadata.favorite == self.favorite
    }

    @objc dynamic var account = ""
    @objc dynamic var assetLocalIdentifier = ""
    @objc dynamic var checksums = ""
    @objc dynamic var chunk: Bool = false
    @objc dynamic var classFile = ""
    @objc dynamic var commentsUnread: Bool = false
    @objc dynamic var contentType = ""
    @objc dynamic var creationDate = NSDate()
    @objc dynamic var dataFingerprint = ""
    @objc dynamic var date = NSDate()
    @objc dynamic var directory: Bool = false
    @objc dynamic var deleteAssetLocalIdentifier: Bool = false
    @objc dynamic var downloadURL = ""
    @objc dynamic var e2eEncrypted: Bool = false
    @objc dynamic var edited: Bool = false
    @objc dynamic var etag = ""
    @objc dynamic var etagResource = ""
    @objc dynamic var favorite: Bool = false
    @objc dynamic var fileId = ""
    @objc dynamic var fileName = ""
    @objc dynamic var fileNameView = ""
    @objc dynamic var hasPreview: Bool = false
    @objc dynamic var iconName = ""
    @objc dynamic var iconUrl = ""
    @objc dynamic var isExtractFile: Bool = false
    @objc dynamic var livePhoto: Bool = false
    @objc dynamic var mountType = ""
    @objc dynamic var name = "" // for unifiedSearch is the provider.id
    @objc dynamic var note = ""
    @objc dynamic var ocId = ""
    @objc dynamic var ownerId = ""
    @objc dynamic var ownerDisplayName = ""
    @objc public var lock = false
    @objc public var lockOwner = ""
    @objc public var lockOwnerEditor = ""
    @objc public var lockOwnerType = 0
    @objc public var lockOwnerDisplayName = ""
    @objc public var lockTime: Date?
    @objc public var lockTimeOut: Date?
    @objc dynamic var path = ""
    @objc dynamic var permissions = ""
    @objc dynamic var quotaUsedBytes: Int64 = 0
    @objc dynamic var quotaAvailableBytes: Int64 = 0
    @objc dynamic var resourceType = ""
    @objc dynamic var richWorkspace: String?
    @objc dynamic var serverUrl = "" // For parent directory!!
    @objc dynamic var session = ""
    @objc dynamic var sessionError = ""
    @objc dynamic var sessionSelector = ""
    @objc dynamic var sessionTaskIdentifier: Int = 0
    @objc dynamic var sharePermissionsCollaborationServices: Int = 0
    let sharePermissionsCloudMesh = List<String>()
    let shareType = List<Int>()
    @objc dynamic var size: Int64 = 0
    @objc dynamic var status: Int = 0
    @objc dynamic var subline: String?
    @objc dynamic var trashbinFileName = ""
    @objc dynamic var trashbinOriginalLocation = ""
    @objc dynamic var trashbinDeletionTime = NSDate()
    @objc dynamic var uploadDate = NSDate()
    @objc dynamic var url = ""
    @objc dynamic var urlBase = ""
    @objc dynamic var user = ""
    @objc dynamic var userId = ""

    override static func primaryKey() -> String {
        return "ocId"
    }
}

class NextcloudDirectoryMetadataTable: Object {
    @objc dynamic var account = ""
    @objc dynamic var colorFolder: String?
    @objc dynamic var e2eEncrypted: Bool = false
    @objc dynamic var etag = ""
    @objc dynamic var favorite: Bool = false
    @objc dynamic var fileId = ""
    @objc dynamic var ocId = ""
    @objc dynamic var offline: Bool = false
    @objc dynamic var permissions = ""
    @objc dynamic var richWorkspace: String?
    @objc dynamic var serverUrl = ""
    @objc dynamic var parentDirectoryServerUrl = ""

    override static func primaryKey() -> String {
        return "ocId"
    }
}

class NextcloudLocalFileMetadataTable: Object {
    @objc dynamic var account = ""
    @objc dynamic var etag = ""
    @objc dynamic var exifDate: NSDate?
    @objc dynamic var exifLatitude = ""
    @objc dynamic var exifLongitude = ""
    @objc dynamic var exifLensModel: String?
    @objc dynamic var favorite: Bool = false
    @objc dynamic var fileName = ""
    @objc dynamic var ocId = ""
    @objc dynamic var offline: Bool = false

    override static func primaryKey() -> String {
        return "ocId"
    }
}
