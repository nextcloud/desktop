//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Tables and indexes of the metadata database, applied through a `DatabaseMigrator`.
///
/// Every table mirrors one record type under `Database/Records`. Dates are `REAL` seconds since the reference date, booleans `INTEGER`, arrays JSON `TEXT`.
///
enum DatabaseSchema {
    /// File extension of the database next to the domain's logs.
    static let fileExtension = "sqlite"

    /// Names of every table, for tests which empty the store.
    static let tableNames = [
        ItemMetadataRecord.databaseTableName,
        ExcludedFromSyncItemRecord.databaseTableName,
        RemoteFileChunkRecord.databaseTableName,
        PendingChunkUploadCleanupRecord.databaseTableName,
        ChangeDeliverySessionRecord.databaseTableName,
        ChangeDeliveryItemRecord.databaseTableName
    ]

    static var migrator: DatabaseMigrator {
        var migrator = DatabaseMigrator()

        migrator.registerMigration("v1_initial") { db in
            try db.create(table: ItemMetadataRecord.databaseTableName) { table in
                table.primaryKey("ocId", .text)

                for name in [
                    "account", "checksums", "classFile", "contentType", "dataFingerprint", "downloadURL", "etag",
                    "fileId", "fileName", "fileNameView", "iconName", "iconUrl", "mountType", "name", "note",
                    "ownerId", "ownerDisplayName", "path", "permissions", "resourceType", "serverUrl",
                    "trashbinFileName", "trashbinOriginalLocation", "urlBase", "user", "userId",
                    "normalizedServerUrl", "normalizedFileName"
                ] {
                    table.column(name, .text).notNull().defaults(to: "")
                }

                for name in [
                    "chunkUploadId", "fileProviderContentVersion", "livePhotoFile", "lockOwner", "lockOwnerEditor",
                    "lockOwnerDisplayName", "lockToken", "richWorkspace", "session", "sessionError"
                ] {
                    table.column(name, .text)
                }

                for name in [
                    "commentsUnread", "deleted", "directory", "e2eEncrypted", "favorite", "hasPreview", "hidden",
                    "isLockFileOfLocalOrigin", "lock", "downloaded", "uploaded", "keepDownloaded", "visitedDirectory"
                ] {
                    table.column(name, .boolean).notNull().defaults(to: false)
                }

                for name in ["creationDate", "date", "syncTime", "trashbinDeletionTime", "uploadDate"] {
                    table.column(name, .real).notNull()
                }

                table.column("lockTime", .real)
                table.column("lockTimeOut", .real)

                for name in ["sharePermissionsCollaborationServices", "status", "quotaUsedBytes", "quotaAvailableBytes", "size"] {
                    table.column(name, .integer).notNull().defaults(to: 0)
                }

                table.column("lockOwnerType", .integer)
                table.column("sessionTaskIdentifier", .integer)

                for name in ["shareType", "sharePermissionsCloudMesh", "tags"] {
                    table.column(name, .text).notNull().defaults(to: "[]")
                }
            }

            // Serves the location lookup (both columns) and the directory lookups (leading column, equality and range).
            try db.create(
                index: "itemMetadata_on_location",
                on: ItemMetadataRecord.databaseTableName,
                columns: ["normalizedServerUrl", "normalizedFileName"]
            )

            // Serves the push-notification lookup by server file identifier, which arrives in batches of thousands.
            try db.create(
                index: "itemMetadata_on_fileId",
                on: ItemMetadataRecord.databaseTableName,
                columns: ["fileId"]
            )

            try db.create(table: ExcludedFromSyncItemRecord.databaseTableName) { table in
                table.primaryKey("ocId", .text)
            }

            try db.create(table: RemoteFileChunkRecord.databaseTableName) { table in
                table.column("remoteChunkStoreFolderName", .text).notNull()
                table.column("fileName", .text).notNull()
                table.column("size", .integer).notNull()
                table.primaryKey(["remoteChunkStoreFolderName", "fileName"])
            }

            try db.create(table: PendingChunkUploadCleanupRecord.databaseTableName) { table in
                table.primaryKey("uploadIdentifier", .text)
            }

            try db.create(table: ChangeDeliverySessionRecord.databaseTableName) { table in
                table.primaryKey("sessionId", .text)
                table.column("containerKey", .text).notNull()
                table.column("currentAnchorKey", .text).notNull()
                table.column("nextSequence", .integer).notNull().defaults(to: 0)
                table.column("finalAnchorRawValue", .blob).notNull()
                table.column("incomplete", .boolean).notNull().defaults(to: false)
                table.column("completed", .boolean).notNull().defaults(to: false)
                table.column("pendingEndSequence", .integer).notNull().defaults(to: 0)
                table.column("pendingAnchorKey", .text)
                table.column("pendingMoreComing", .boolean).notNull().defaults(to: false)
                table.column("pendingReported", .boolean).notNull().defaults(to: false)
                table.column("hardRemoveDeleted", .boolean).notNull().defaults(to: false)
            }

            try db.create(
                index: "changeDeliverySession_on_containerKey",
                on: ChangeDeliverySessionRecord.databaseTableName,
                columns: ["containerKey"]
            )

            try db.create(table: ChangeDeliveryItemRecord.databaseTableName, options: [.withoutRowID]) { table in
                table.column("sessionId", .text).notNull()
                table.column("sequence", .integer).notNull()
                table.column("metadataData", .blob).notNull()
                table.column("deleted", .boolean).notNull().defaults(to: false)
                table.primaryKey(["sessionId", "sequence"])
            }
        }

        return migrator
    }
}
