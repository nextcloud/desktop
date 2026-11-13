//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
import NextcloudKit

///
/// Row of the `itemMetadata` table.
///
/// Stores item metadata and the normalized parent and filename keys used for lookups.
/// The account fields are supplied in memory when reading a row.
///
/// Rows are read and written by the explicit conversion in `ItemMetadataRecord+Row.swift`: dates as seconds since the reference date, arrays as JSON text.
///
struct ItemMetadataRecord: ItemMetadata, Equatable, Sendable, PersistableRecord {
    enum CodingKeys: String, CodingKey, ColumnExpression, CaseIterable {
        case ocId
        case checksums
        case chunkUploadId
        case classFile
        case commentsUnread
        case contentType
        case creationDate
        case dataFingerprint
        case date
        case davRoot
        case deleted
        case directory
        case downloaded
        case downloadURL
        case e2eEncrypted
        case etag
        case favorite
        case fileId
        case fileName
        case fileNameView
        case fileProviderContentVersion
        case hasPreview
        case hidden
        case iconName
        case iconUrl
        case isLockFileOfLocalOrigin
        case keepDownloaded
        case livePhotoFile
        case lock
        case lockOwner
        case lockOwnerDisplayName
        case lockOwnerEditor
        case lockOwnerType
        case lockTime
        case lockTimeOut
        case lockToken
        case mountType
        case name
        case normalizedFileName
        case normalizedParentPath
        case note
        case ownerDisplayName
        case ownerId
        case parentPath
        case permissions
        case quotaAvailableBytes
        case quotaUsedBytes
        case resourceType
        case richWorkspace
        case session
        case sessionError
        case sessionTaskIdentifier
        case sharePermissionsCloudMesh
        case sharePermissionsCollaborationServices
        case shareType
        case size
        case status
        case syncTime
        case tags
        case trashbinDeletionTime
        case trashbinFileName
        case trashbinOriginalLocation
        case uploadDate
        case uploaded
        case visitedDirectory
    }

    static let databaseTableName = "itemMetadata"

    var ocId: String
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
    var permissions: String
    var quotaUsedBytes: Int64
    var quotaAvailableBytes: Int64
    var resourceType: String
    var richWorkspace: String?
    /// Full parent URL used by WebDAV requests.
    var serverUrl: String

    /// The Files or Trash root and the path of the parent beneath it.
    var storedDAVParent: DAVPath
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

    // These fields supply the account details required by ItemMetadata in memory.
    var account: String
    var urlBase: String
    var user: String
    var userId: String

    /// The path below the saved DAV root.
    var parentPath: String { storedDAVParent.path }

    /// NFC form of `parentPath`, maintained by this type.
    var normalizedParentPath: String

    /// NFC form of `fileName`, maintained by this type.
    var normalizedFileName: String

    /// Columns whose stored value could not be decoded and were replaced by their empty value. Not stored; filled when a row is read.
    var unreadableColumns: [String] = []

    init(
        _ value: any ItemMetadata,
        domainAccount: Account? = nil
    ) throws {
        let davRootComponents = domainAccount.map { (serverBaseURL: $0.serverUrl, backendUserID: $0.id) }
            ?? Self.serverBaseURLAndBackendUserID(for: value)
        // Use the root placeholder even if the server names the Files root after the backend user ID.
        guard let davRootComponents,
              let normalizedParent = Account.normalizedDAVParent(
                  parentURL: value.serverUrl,
                  fileName: value.fileName,
                  accountBaseURL: davRootComponents.serverBaseURL,
                  backendUserID: davRootComponents.backendUserID
              )
        else { throw NKError.invalidData }

        ocId = value.ocId
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
        downloadURL = value.downloadURL
        e2eEncrypted = value.e2eEncrypted
        etag = value.etag
        fileProviderContentVersion = value.fileProviderContentVersion
        favorite = value.favorite
        fileId = value.fileId
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
        permissions = value.permissions
        quotaUsedBytes = value.quotaUsedBytes
        quotaAvailableBytes = value.quotaAvailableBytes
        resourceType = value.resourceType
        richWorkspace = value.richWorkspace
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

        directory = normalizedParent.isFilesRoot || value.directory
        fileName = normalizedParent.isFilesRoot ? SendableItemMetadata.filesRootPlaceholderName : value.fileName
        fileNameView = normalizedParent.isFilesRoot ? SendableItemMetadata.filesRootPlaceholderName : value.fileNameView
        storedDAVParent = normalizedParent.parent
        serverUrl = Account.absoluteURL(
            for: normalizedParent.parent,
            accountBaseURL: davRootComponents.serverBaseURL,
            backendUserID: davRootComponents.backendUserID
        )
        normalizedParentPath = normalizedParent.parent.path.precomposedStringWithCanonicalMapping
        normalizedFileName = fileName.precomposedStringWithCanonicalMapping

        account = domainAccount?.ncKitAccount ?? value.account
        urlBase = davRootComponents.serverBaseURL
        userId = davRootComponents.backendUserID
        user = domainAccount?.username ?? value.user
        if domainAccount == nil,
           user.isEmpty,
           account.hasSuffix(" " + davRootComponents.serverBaseURL) {
            user = String(account.dropLast(davRootComponents.serverBaseURL.count + 1))
        }
    }

    /// Find the DAV root recorded in metadata from Realm or a queued change.
    static func serverBaseURLAndBackendUserID(for metadata: any ItemMetadata) -> (serverBaseURL: String, backendUserID: String)? {
        if !metadata.urlBase.isEmpty, !metadata.userId.isEmpty {
            return (metadata.urlBase, metadata.userId)
        }
        return recoverServerBaseAndUserIDForLegacyContainer(
            ocId: metadata.ocId,
            account: metadata.account,
            parentURL: metadata.serverUrl
        )
    }

    /// The row as the value type handed to the rest of the extension.
    var metadata: SendableItemMetadata {
        SendableItemMetadata(value: self)
    }

    /// Update the parent URL and filename together with their SQLite lookup keys.
    /// The domain account supplies the WebDAV Files and Trash roots used to save the parent.
    mutating func updateLocation(parentURL: String, fileName: String, domainAccount: Account) throws {
        guard let parent = domainAccount.davPath(forURL: parentURL) else { throw NKError.invalidData }

        serverUrl = Account.absoluteURL(for: parent, accountBaseURL: domainAccount.serverUrl, backendUserID: domainAccount.id)
        self.fileName = fileName
        storedDAVParent = parent
        normalizedParentPath = parentPath.precomposedStringWithCanonicalMapping
        normalizedFileName = fileName.precomposedStringWithCanonicalMapping
    }
}
