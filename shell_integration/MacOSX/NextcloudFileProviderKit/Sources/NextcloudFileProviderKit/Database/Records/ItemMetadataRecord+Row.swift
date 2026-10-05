//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Explicit row conversion for ``ItemMetadataRecord``.
///
/// The generic `Codable` path builds a keyed container and a fresh JSON coder for every array column of every row, which dominates the cost of a large directory write. Reading and writing the columns directly keeps that path cheap. Dates are stored as seconds since the reference date; arrays as JSON text.
///
extension ItemMetadataRecord {
    /// JSONEncoder and JSONDecoder keep no state across calls, so one instance serves every row.
    private nonisolated(unsafe) static let jsonEncoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        return encoder
    }()

    private nonisolated(unsafe) static let jsonDecoder = JSONDecoder()

    private static func encodeArray(_ values: some Encodable) throws -> String {
        try String(decoding: jsonEncoder.encode(values), as: UTF8.self)
    }

    private static func decodeArray<T: Decodable>(_ text: String) throws -> T {
        try jsonDecoder.decode(T.self, from: Data(text.utf8))
    }

    init(row: Row) throws {
        ocId = row["ocId"]
        account = row["account"]
        checksums = row["checksums"]
        chunkUploadId = row["chunkUploadId"]
        classFile = row["classFile"]
        commentsUnread = row["commentsUnread"]
        contentType = row["contentType"]
        creationDate = Date(timeIntervalSinceReferenceDate: row["creationDate"])
        dataFingerprint = row["dataFingerprint"]
        date = Date(timeIntervalSinceReferenceDate: row["date"])
        syncTime = Date(timeIntervalSinceReferenceDate: row["syncTime"])
        deleted = row["deleted"]
        directory = row["directory"]
        downloadURL = row["downloadURL"]
        e2eEncrypted = row["e2eEncrypted"]
        etag = row["etag"]
        fileProviderContentVersion = row["fileProviderContentVersion"]
        favorite = row["favorite"]
        fileId = row["fileId"]
        fileName = row["fileName"]
        fileNameView = row["fileNameView"]
        hasPreview = row["hasPreview"]
        hidden = row["hidden"]
        iconName = row["iconName"]
        iconUrl = row["iconUrl"]
        isLockFileOfLocalOrigin = row["isLockFileOfLocalOrigin"]
        mountType = row["mountType"]
        name = row["name"]
        note = row["note"]
        ownerId = row["ownerId"]
        ownerDisplayName = row["ownerDisplayName"]
        livePhotoFile = row["livePhotoFile"]
        lock = row["lock"]
        lockOwner = row["lockOwner"]
        lockOwnerEditor = row["lockOwnerEditor"]
        lockOwnerType = row["lockOwnerType"]
        lockOwnerDisplayName = row["lockOwnerDisplayName"]
        lockTime = (row["lockTime"] as Double?).map(Date.init(timeIntervalSinceReferenceDate:))
        lockTimeOut = (row["lockTimeOut"] as Double?).map(Date.init(timeIntervalSinceReferenceDate:))
        lockToken = row["lockToken"]
        path = row["path"]
        permissions = row["permissions"]
        quotaUsedBytes = row["quotaUsedBytes"]
        quotaAvailableBytes = row["quotaAvailableBytes"]
        resourceType = row["resourceType"]
        richWorkspace = row["richWorkspace"]
        serverUrl = row["serverUrl"]
        session = row["session"]
        sessionError = row["sessionError"]
        sessionTaskIdentifier = row["sessionTaskIdentifier"]
        sharePermissionsCollaborationServices = row["sharePermissionsCollaborationServices"]
        sharePermissionsCloudMesh = try Self.decodeArray(row["sharePermissionsCloudMesh"])
        shareType = try Self.decodeArray(row["shareType"])
        size = row["size"]
        status = row["status"]
        tags = try Self.decodeArray(row["tags"])
        downloaded = row["downloaded"]
        uploaded = row["uploaded"]
        keepDownloaded = row["keepDownloaded"]
        visitedDirectory = row["visitedDirectory"]
        trashbinFileName = row["trashbinFileName"]
        trashbinOriginalLocation = row["trashbinOriginalLocation"]
        trashbinDeletionTime = Date(timeIntervalSinceReferenceDate: row["trashbinDeletionTime"])
        uploadDate = Date(timeIntervalSinceReferenceDate: row["uploadDate"])
        urlBase = row["urlBase"]
        user = row["user"]
        userId = row["userId"]
        normalizedServerUrl = row["normalizedServerUrl"]
        normalizedFileName = row["normalizedFileName"]
    }

    func encode(to container: inout PersistenceContainer) throws {
        container["ocId"] = ocId
        container["account"] = account
        container["checksums"] = checksums
        container["chunkUploadId"] = chunkUploadId
        container["classFile"] = classFile
        container["commentsUnread"] = commentsUnread
        container["contentType"] = contentType
        container["creationDate"] = creationDate.timeIntervalSinceReferenceDate
        container["dataFingerprint"] = dataFingerprint
        container["date"] = date.timeIntervalSinceReferenceDate
        container["syncTime"] = syncTime.timeIntervalSinceReferenceDate
        container["deleted"] = deleted
        container["directory"] = directory
        container["downloadURL"] = downloadURL
        container["e2eEncrypted"] = e2eEncrypted
        container["etag"] = etag
        container["fileProviderContentVersion"] = fileProviderContentVersion
        container["favorite"] = favorite
        container["fileId"] = fileId
        container["fileName"] = fileName
        container["fileNameView"] = fileNameView
        container["hasPreview"] = hasPreview
        container["hidden"] = hidden
        container["iconName"] = iconName
        container["iconUrl"] = iconUrl
        container["isLockFileOfLocalOrigin"] = isLockFileOfLocalOrigin
        container["mountType"] = mountType
        container["name"] = name
        container["note"] = note
        container["ownerId"] = ownerId
        container["ownerDisplayName"] = ownerDisplayName
        container["livePhotoFile"] = livePhotoFile
        container["lock"] = lock
        container["lockOwner"] = lockOwner
        container["lockOwnerEditor"] = lockOwnerEditor
        container["lockOwnerType"] = lockOwnerType
        container["lockOwnerDisplayName"] = lockOwnerDisplayName
        container["lockTime"] = lockTime?.timeIntervalSinceReferenceDate
        container["lockTimeOut"] = lockTimeOut?.timeIntervalSinceReferenceDate
        container["lockToken"] = lockToken
        container["path"] = path
        container["permissions"] = permissions
        container["quotaUsedBytes"] = quotaUsedBytes
        container["quotaAvailableBytes"] = quotaAvailableBytes
        container["resourceType"] = resourceType
        container["richWorkspace"] = richWorkspace
        container["serverUrl"] = serverUrl
        container["session"] = session
        container["sessionError"] = sessionError
        container["sessionTaskIdentifier"] = sessionTaskIdentifier
        container["sharePermissionsCollaborationServices"] = sharePermissionsCollaborationServices
        container["sharePermissionsCloudMesh"] = try Self.encodeArray(sharePermissionsCloudMesh)
        container["shareType"] = try Self.encodeArray(shareType)
        container["size"] = size
        container["status"] = status
        container["tags"] = try Self.encodeArray(tags)
        container["downloaded"] = downloaded
        container["uploaded"] = uploaded
        container["keepDownloaded"] = keepDownloaded
        container["visitedDirectory"] = visitedDirectory
        container["trashbinFileName"] = trashbinFileName
        container["trashbinOriginalLocation"] = trashbinOriginalLocation
        container["trashbinDeletionTime"] = trashbinDeletionTime.timeIntervalSinceReferenceDate
        container["uploadDate"] = uploadDate.timeIntervalSinceReferenceDate
        container["urlBase"] = urlBase
        container["user"] = user
        container["userId"] = userId
        container["normalizedServerUrl"] = normalizedServerUrl
        container["normalizedFileName"] = normalizedFileName
    }

    /// The column values in ``Columns`` order, for statements prepared once and bound per row.
    func databaseValues() throws -> [(any DatabaseValueConvertible)?] {
        try [
            ocId,
            account,
            checksums,
            chunkUploadId,
            classFile,
            commentsUnread,
            contentType,
            creationDate.timeIntervalSinceReferenceDate,
            dataFingerprint,
            date.timeIntervalSinceReferenceDate,
            syncTime.timeIntervalSinceReferenceDate,
            deleted,
            directory,
            downloadURL,
            e2eEncrypted,
            etag,
            fileProviderContentVersion,
            favorite,
            fileId,
            fileName,
            fileNameView,
            hasPreview,
            hidden,
            iconName,
            iconUrl,
            isLockFileOfLocalOrigin,
            mountType,
            name,
            note,
            ownerId,
            ownerDisplayName,
            livePhotoFile,
            lock,
            lockOwner,
            lockOwnerEditor,
            lockOwnerType,
            lockOwnerDisplayName,
            lockTime?.timeIntervalSinceReferenceDate,
            lockTimeOut?.timeIntervalSinceReferenceDate,
            lockToken,
            path,
            permissions,
            quotaUsedBytes,
            quotaAvailableBytes,
            resourceType,
            richWorkspace,
            serverUrl,
            session,
            sessionError,
            sessionTaskIdentifier,
            sharePermissionsCollaborationServices,
            Self.encodeArray(sharePermissionsCloudMesh),
            Self.encodeArray(shareType),
            size,
            status,
            Self.encodeArray(tags),
            downloaded,
            uploaded,
            keepDownloaded,
            visitedDirectory,
            trashbinFileName,
            trashbinOriginalLocation,
            trashbinDeletionTime.timeIntervalSinceReferenceDate,
            uploadDate.timeIntervalSinceReferenceDate,
            urlBase,
            user,
            userId,
            normalizedServerUrl,
            normalizedFileName
        ]
    }

    /// `INSERT ... ON CONFLICT(ocId) DO UPDATE` over every column, prepared once per connection.
    private static let upsertSQL: String = {
        let columns = Columns.allCases.map(\.rawValue)
        let assignments = columns.dropFirst().map { "\($0) = excluded.\($0)" }
        return "INSERT INTO \(databaseTableName) (\(columns.joined(separator: ", "))) VALUES (\(columns.map { _ in "?" }.joined(separator: ", "))) ON CONFLICT(ocId) DO UPDATE SET \(assignments.joined(separator: ", "))"
    }()

    /// Insert or replace the row through a cached statement.
    ///
    /// Equivalent to `upsert(_:)`, which rebuilds its statement text from the persistence container on every call; on a write of thousands of rows that rebuilding costs more than the database work.
    ///
    func upsertRow(_ db: Database) throws {
        let statement = try db.cachedStatement(sql: Self.upsertSQL)
        try statement.execute(arguments: StatementArguments(databaseValues()))
    }
}
