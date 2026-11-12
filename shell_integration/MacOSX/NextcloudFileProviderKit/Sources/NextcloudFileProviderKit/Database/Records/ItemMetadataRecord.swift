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
    var permissions: String
    var quotaUsedBytes: Int64
    var quotaAvailableBytes: Int64
    var resourceType: String
    var richWorkspace: String?
    /// Full parent URL used by WebDAV requests.
    var serverUrl: String

    /// Parent path relative to the account's DAV root, such as files:/Docs or trash:.
    /// A legacy row whose DAV root could not be identified has no relative path.
    var storedParentPath: String?
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

    /// NFC form of the stored parent location, maintained by this type.
    var normalizedServerUrl: String

    /// NFC form of `fileName`, maintained by this type.
    var normalizedFileName: String

    /// Columns whose stored value could not be decoded and were replaced by their empty value. Not stored; filled when a row is read.
    var unreadableColumns: [String] = []

    enum CodingKeys: String, CodingKey, ColumnExpression, CaseIterable {
        case ocId
        case account
        case checksums
        case chunkUploadId
        case classFile
        case commentsUnread
        case contentType
        case creationDate
        case dataFingerprint
        case date
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
        case normalizedServerUrl
        case note
        case ownerDisplayName
        case ownerId
        case permissions
        case quotaAvailableBytes
        case quotaUsedBytes
        case resourceType
        case richWorkspace
        case serverUrl
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
        case urlBase
        case user
        case userId
        case visitedDirectory
    }

    init(_ value: any ItemMetadata, domainAccount: Account? = nil) {
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

        // Save a parent relative to the account's Files or Trash root when it can be identified.
        // Use the root placeholder even if the server names the Files root after the backend user ID.
        let davRootComponents = Self.serverBaseURLAndBackendUserID(for: value, domainAccount: domainAccount)
        let baseURL = davRootComponents?.serverBaseURL ?? value.urlBase
        let backendUserID = davRootComponents?.backendUserID ?? value.userId
        let isRoot = davRootComponents != nil
            && Account.isFilesRootAddress(
                parentURL: value.serverUrl,
                fileName: value.fileName,
                accountBaseURL: baseURL,
                backendUserID: backendUserID,
                acceptEquivalentResponseWebroot: true
            )
        directory = isRoot || value.directory
        fileName = isRoot ? SendableItemMetadata.filesRootPlaceholderName : value.fileName
        fileNameView = isRoot ? SendableItemMetadata.filesRootPlaceholderName : value.fileNameView
        urlBase = baseURL
        userId = backendUserID
        user = value.user
        if user.isEmpty, let davRootComponents, account.hasSuffix(" " + davRootComponents.serverBaseURL) {
            user = String(account.dropLast(davRootComponents.serverBaseURL.count + 1))
        }
        let relativeParentPath: String? = if isRoot {
            Account.storedFilesRootLocation
        } else if davRootComponents != nil {
            Account.davRelativePath(
                forURL: value.serverUrl,
                accountBaseURL: baseURL,
                backendUserID: backendUserID,
                acceptEquivalentResponseWebroot: true
            )
        } else {
            nil
        }
        storedParentPath = relativeParentPath
        serverUrl = relativeParentPath.flatMap {
            Account.absoluteURL(fromDAVRelativePath: $0, accountBaseURL: baseURL, backendUserID: backendUserID)
        } ?? value.serverUrl
        normalizedServerUrl = (relativeParentPath ?? value.serverUrl).precomposedStringWithCanonicalMapping
        normalizedFileName = fileName.precomposedStringWithCanonicalMapping
    }

    /// Find the server base URL and backend user ID that identify this item's DAV roots.
    /// The manager can fill in missing values for items that match its account.
    static func serverBaseURLAndBackendUserID(
        for metadata: any ItemMetadata,
        domainAccount: Account?
    ) -> (serverBaseURL: String, backendUserID: String)? {
        if let domainAccount {
            guard metadata.account == domainAccount.ncKitAccount,
                  metadata.urlBase.isEmpty || metadata.urlBase.utf8.elementsEqual(domainAccount.serverUrl.utf8),
                  metadata.userId.isEmpty || metadata.userId.utf8.elementsEqual(domainAccount.id.utf8)
            else { return nil }
        }
        if !metadata.urlBase.isEmpty, !metadata.userId.isEmpty {
            return (metadata.urlBase, metadata.userId)
        }
        if let domainAccount,
           domainAccount.davRelativePath(forURL: metadata.serverUrl) != nil
            || Account.isFilesRootAddress(
                parentURL: metadata.serverUrl,
                fileName: metadata.fileName,
                accountBaseURL: domainAccount.serverUrl,
                backendUserID: domainAccount.id
            ) {
            return (domainAccount.serverUrl, domainAccount.id)
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
    mutating func updateLocation(parentURL: String, fileName: String, domainAccount: Account) {
        serverUrl = parentURL
        self.fileName = fileName
        storedParentPath = domainAccount.davRelativePath(forURL: parentURL)
        if storedParentPath != nil {
            account = domainAccount.ncKitAccount
            urlBase = domainAccount.serverUrl
            user = domainAccount.username
            userId = domainAccount.id
        }
        normalizedServerUrl = (storedParentPath ?? parentURL).precomposedStringWithCanonicalMapping
        normalizedFileName = fileName.precomposedStringWithCanonicalMapping
    }
}
