//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Row of the `itemMetadata` table.
///
/// Carries the fields of ``SendableItemMetadata`` plus the two normalized location keys the location queries run on. The keys follow `serverUrl` and `fileName` whenever those change through this type.
///
/// Rows are read and written by the explicit conversion in `ItemMetadataRecord+Row.swift`: dates as seconds since the reference date, arrays as JSON text.
///
struct ItemMetadataRecord: ItemMetadata, Equatable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "itemMetadata"

    var ocId: String
    var account: String
    var checksums: String
    var chunkUploadId: String?
    var classFile: String
    var commentsUnread: Bool
    var contentType: String
    var creationDate: Date
    var dataFingerprint: String
    var date: Date
    var syncTime: Date
    var deleted: Bool
    var directory: Bool
    var downloadURL: String
    var e2eEncrypted: Bool
    var etag: String
    var fileProviderContentVersion: String?
    var favorite: Bool
    var fileId: String
    var fileName: String
    var fileNameView: String
    var hasPreview: Bool
    var hidden: Bool
    var iconName: String
    var iconUrl: String
    var isLockFileOfLocalOrigin: Bool
    var mountType: String
    var name: String
    var note: String
    var ownerId: String
    var ownerDisplayName: String
    var livePhotoFile: String?
    var lock: Bool
    var lockOwner: String?
    var lockOwnerEditor: String?
    var lockOwnerType: Int?
    var lockOwnerDisplayName: String?
    var lockTime: Date?
    var lockTimeOut: Date?
    var lockToken: String?
    var path: String
    var permissions: String
    var quotaUsedBytes: Int64
    var quotaAvailableBytes: Int64
    var resourceType: String
    var richWorkspace: String?
    var serverUrl: String
    var session: String?
    var sessionError: String?
    var sessionTaskIdentifier: Int?
    var sharePermissionsCollaborationServices: Int
    var sharePermissionsCloudMesh: [String]
    var shareType: [Int]
    var size: Int64
    var status: Int
    var tags: [String]
    var downloaded: Bool
    var uploaded: Bool
    var keepDownloaded: Bool
    var visitedDirectory: Bool
    var trashbinFileName: String
    var trashbinOriginalLocation: String
    var trashbinDeletionTime: Date
    var uploadDate: Date
    var urlBase: String
    var user: String
    var userId: String

    /// NFC form of `serverUrl`, maintained by this type.
    var normalizedServerUrl: String

    /// NFC form of `fileName`, maintained by this type.
    var normalizedFileName: String

    enum CodingKeys: String, CodingKey, ColumnExpression, CaseIterable {
        case ocId, account, checksums, chunkUploadId, classFile, commentsUnread
        case contentType, creationDate, dataFingerprint, date, syncTime, deleted
        case directory, downloadURL, e2eEncrypted, etag, fileProviderContentVersion, favorite
        case fileId, fileName, fileNameView, hasPreview, hidden, iconName
        case iconUrl, isLockFileOfLocalOrigin, mountType, name, note, ownerId
        case ownerDisplayName, livePhotoFile, lock, lockOwner, lockOwnerEditor, lockOwnerType
        case lockOwnerDisplayName, lockTime, lockTimeOut, lockToken, path, permissions
        case quotaUsedBytes, quotaAvailableBytes, resourceType, richWorkspace, serverUrl, session
        case sessionError, sessionTaskIdentifier, sharePermissionsCollaborationServices, sharePermissionsCloudMesh, shareType, size
        case status, tags, downloaded, uploaded, keepDownloaded, visitedDirectory
        case trashbinFileName, trashbinOriginalLocation, trashbinDeletionTime, uploadDate, urlBase, user
        case userId, normalizedServerUrl, normalizedFileName
    }

    init(_ value: any ItemMetadata) {
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
        fileProviderContentVersion = value.fileProviderContentVersion
        favorite = value.favorite
        fileId = value.fileId
        fileName = value.fileName
        fileNameView = value.fileNameView
        hasPreview = value.hasPreview
        hidden = value.hidden
        iconName = value.iconName
        iconUrl = value.iconUrl
        isLockFileOfLocalOrigin = value.isLockFileOfLocalOrigin
        mountType = value.mountType
        name = value.name
        note = value.note
        ownerId = value.ownerId
        ownerDisplayName = value.ownerDisplayName
        livePhotoFile = value.livePhotoFile
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
        normalizedServerUrl = value.serverUrl.precomposedStringWithCanonicalMapping
        normalizedFileName = value.fileName.precomposedStringWithCanonicalMapping
    }

    /// The row as the value type handed to the rest of the extension.
    var metadata: SendableItemMetadata {
        SendableItemMetadata(value: self)
    }

    /// Set the raw location and its normalized keys together.
    mutating func updateLocation(serverUrl: String, fileName: String) {
        self.serverUrl = serverUrl
        self.fileName = fileName
        normalizedServerUrl = serverUrl.precomposedStringWithCanonicalMapping
        normalizedFileName = fileName.precomposedStringWithCanonicalMapping
    }
}
