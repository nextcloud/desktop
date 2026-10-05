//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Explicit row conversion for ``ItemMetadataRecord``.
///
/// The generic `Codable` path builds a keyed container and a fresh JSON coder for every array column of every row, which dominates the cost of a large directory write. Reading and writing the columns directly keeps that path cheap. Dates are stored as seconds since the reference date; arrays as JSON text.
///
/// Every column is decoded through the throwing `Row.decode`, so a damaged value makes the row undecodable instead of stopping the process. Array columns are the exception: a damaged list decodes as empty and is reported through ``ItemMetadataRecord/unreadableColumns``.
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

    /// Decode a JSON array column, treating an unreadable value as empty and recording the column so the caller can log it. One damaged tag list must not take the whole row, or the whole query, with it.
    private static func decodeArray<T: Decodable & ExpressibleByArrayLiteral>(_ row: Row, column: String, into unreadableColumns: inout [String]) -> T {
        do {
            let text: String = try row.decode(forColumn: column)
            return try jsonDecoder.decode(T.self, from: Data(text.utf8))
        } catch {
            unreadableColumns.append(column)
            return []
        }
    }

    init(row: Row) throws {
        ocId = try row.decode(forColumn: "ocId")
        account = try row.decode(forColumn: "account")
        checksums = try row.decode(forColumn: "checksums")
        chunkUploadId = try row.decode(forColumn: "chunkUploadId")
        classFile = try row.decode(forColumn: "classFile")
        commentsUnread = try row.decode(forColumn: "commentsUnread")
        contentType = try row.decode(forColumn: "contentType")
        creationDate = try Date(timeIntervalSinceReferenceDate: row.decode(forColumn: "creationDate"))
        dataFingerprint = try row.decode(forColumn: "dataFingerprint")
        date = try Date(timeIntervalSinceReferenceDate: row.decode(forColumn: "date"))
        syncTime = try Date(timeIntervalSinceReferenceDate: row.decode(forColumn: "syncTime"))
        deleted = try row.decode(forColumn: "deleted")
        directory = try row.decode(forColumn: "directory")
        downloadURL = try row.decode(forColumn: "downloadURL")
        e2eEncrypted = try row.decode(forColumn: "e2eEncrypted")
        etag = try row.decode(forColumn: "etag")
        fileProviderContentVersion = try row.decode(forColumn: "fileProviderContentVersion")
        favorite = try row.decode(forColumn: "favorite")
        fileId = try row.decode(forColumn: "fileId")
        fileName = try row.decode(forColumn: "fileName")
        fileNameView = try row.decode(forColumn: "fileNameView")
        hasPreview = try row.decode(forColumn: "hasPreview")
        hidden = try row.decode(forColumn: "hidden")
        iconName = try row.decode(forColumn: "iconName")
        iconUrl = try row.decode(forColumn: "iconUrl")
        isLockFileOfLocalOrigin = try row.decode(forColumn: "isLockFileOfLocalOrigin")
        mountType = try row.decode(forColumn: "mountType")
        name = try row.decode(forColumn: "name")
        note = try row.decode(forColumn: "note")
        ownerId = try row.decode(forColumn: "ownerId")
        ownerDisplayName = try row.decode(forColumn: "ownerDisplayName")
        livePhotoFile = try row.decode(forColumn: "livePhotoFile")
        lock = try row.decode(forColumn: "lock")
        lockOwner = try row.decode(forColumn: "lockOwner")
        lockOwnerEditor = try row.decode(forColumn: "lockOwnerEditor")
        lockOwnerType = try row.decode(forColumn: "lockOwnerType")
        lockOwnerDisplayName = try row.decode(forColumn: "lockOwnerDisplayName")
        lockTime = try (row.decode(Double?.self, forColumn: "lockTime")).map(Date.init(timeIntervalSinceReferenceDate:))
        lockTimeOut = try (row.decode(Double?.self, forColumn: "lockTimeOut")).map(Date.init(timeIntervalSinceReferenceDate:))
        lockToken = try row.decode(forColumn: "lockToken")
        path = try row.decode(forColumn: "path")
        permissions = try row.decode(forColumn: "permissions")
        quotaUsedBytes = try row.decode(forColumn: "quotaUsedBytes")
        quotaAvailableBytes = try row.decode(forColumn: "quotaAvailableBytes")
        resourceType = try row.decode(forColumn: "resourceType")
        richWorkspace = try row.decode(forColumn: "richWorkspace")
        serverUrl = try row.decode(forColumn: "serverUrl")
        session = try row.decode(forColumn: "session")
        sessionError = try row.decode(forColumn: "sessionError")
        sessionTaskIdentifier = try row.decode(forColumn: "sessionTaskIdentifier")
        sharePermissionsCollaborationServices = try row.decode(forColumn: "sharePermissionsCollaborationServices")
        sharePermissionsCloudMesh = Self.decodeArray(row, column: "sharePermissionsCloudMesh", into: &unreadableColumns)
        shareType = Self.decodeArray(row, column: "shareType", into: &unreadableColumns)
        size = try row.decode(forColumn: "size")
        status = try row.decode(forColumn: "status")
        tags = Self.decodeArray(row, column: "tags", into: &unreadableColumns)
        downloaded = try row.decode(forColumn: "downloaded")
        uploaded = try row.decode(forColumn: "uploaded")
        keepDownloaded = try row.decode(forColumn: "keepDownloaded")
        visitedDirectory = try row.decode(forColumn: "visitedDirectory")
        trashbinFileName = try row.decode(forColumn: "trashbinFileName")
        trashbinOriginalLocation = try row.decode(forColumn: "trashbinOriginalLocation")
        trashbinDeletionTime = try Date(timeIntervalSinceReferenceDate: row.decode(forColumn: "trashbinDeletionTime"))
        uploadDate = try Date(timeIntervalSinceReferenceDate: row.decode(forColumn: "uploadDate"))
        urlBase = try row.decode(forColumn: "urlBase")
        user = try row.decode(forColumn: "user")
        userId = try row.decode(forColumn: "userId")
        normalizedServerUrl = try row.decode(forColumn: "normalizedServerUrl")
        normalizedFileName = try row.decode(forColumn: "normalizedFileName")
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
