//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for how an item row is stored and read back.
    ///
    @Suite("Item metadata record")
    struct ItemMetadataRecordTests {
        let manager = DatabaseTestSuites.makeManager()

        /// A row with every optional set and every collection non-empty.
        private func makeFullMetadata() -> SendableItemMetadata {
            var metadata = DatabaseTestSuites.makeFile(ocId: "full", fileName: "Füll.txt", serverUrl: DatabaseTestSuites.account.davFilesUrl + "/Dir")
            metadata.checksums = "sha1:abc"
            metadata.chunkUploadId = "upload-1"
            metadata.classFile = "document"
            metadata.commentsUnread = true
            metadata.contentType = "text/plain"
            metadata.creationDate = Date(timeIntervalSinceReferenceDate: 700_000_000.25)
            metadata.dataFingerprint = "fp"
            metadata.date = Date(timeIntervalSinceReferenceDate: 700_000_001.5)
            metadata.syncTime = Date(timeIntervalSinceReferenceDate: 700_000_002.75)
            metadata.deleted = true
            metadata.downloadURL = "https://dl"
            metadata.e2eEncrypted = true
            metadata.etag = "etag"
            metadata.fileProviderContentVersion = "v1"
            metadata.favorite = true
            metadata.fileId = "42"
            metadata.hasPreview = true
            metadata.hidden = true
            metadata.iconName = "icon"
            metadata.iconUrl = "https://icon"
            metadata.isLockFileOfLocalOrigin = true
            metadata.livePhotoFile = "live"
            metadata.lock = true
            metadata.lockOwner = "owner"
            metadata.lockOwnerEditor = "editor"
            metadata.lockOwnerType = 1
            metadata.lockOwnerDisplayName = "Owner"
            metadata.lockTime = Date(timeIntervalSinceReferenceDate: 700_000_003.125)
            metadata.lockTimeOut = Date(timeIntervalSinceReferenceDate: 700_000_004.0625)
            metadata.lockToken = "token"
            metadata.mountType = "external"
            metadata.note = "note"
            metadata.ownerId = "ownerId"
            metadata.ownerDisplayName = "Owner Name"
            metadata.path = "/path"
            metadata.permissions = "RGDNVW"
            metadata.quotaUsedBytes = 123
            metadata.quotaAvailableBytes = -3
            metadata.resourceType = "collection"
            metadata.richWorkspace = "workspace"
            metadata.session = "session"
            metadata.sessionError = "error"
            metadata.sessionTaskIdentifier = 7
            metadata.sharePermissionsCollaborationServices = 31
            metadata.sharePermissionsCloudMesh = ["read", "write"]
            metadata.shareType = [0, 3]
            metadata.size = 9_876_543_210
            metadata.status = Status.uploadError.rawValue
            metadata.tags = ["a", "b"]
            metadata.downloaded = true
            metadata.uploaded = true
            metadata.keepDownloaded = true
            metadata.visitedDirectory = true
            metadata.trashbinFileName = "trash.txt"
            metadata.trashbinOriginalLocation = "Dir/trash.txt"
            metadata.trashbinDeletionTime = Date(timeIntervalSinceReferenceDate: 700_000_005.5)
            metadata.uploadDate = Date(timeIntervalSinceReferenceDate: 700_000_006.5)
            return metadata
        }

        @Test func everyFieldSurvivesTheRoundTrip() throws {
            let metadata = makeFullMetadata()

            try manager.insertForTesting(metadata)

            let stored = try #require(manager.itemMetadata(ocId: "full"))
            #expect(stored.creationDate.timeIntervalSinceReferenceDate == metadata.creationDate.timeIntervalSinceReferenceDate)
            #expect(stored.date.timeIntervalSinceReferenceDate == metadata.date.timeIntervalSinceReferenceDate)
            #expect(stored == metadata)
            #expect(ItemMetadataRecord(metadata).metadata == metadata)
        }

        @Test func optionalFieldsRoundTripAsNil() throws {
            var metadata = DatabaseTestSuites.makeFile(ocId: "bare", fileName: "bare.txt")
            metadata.chunkUploadId = nil
            metadata.lockTime = nil
            metadata.lockOwnerType = nil

            try manager.insertForTesting(metadata)

            let stored = try #require(manager.itemMetadata(ocId: "bare"))
            #expect(stored.chunkUploadId == nil)
            #expect(stored.lockTime == nil)
            #expect(stored.lockOwnerType == nil)
            #expect(stored == metadata)
        }

        @Test func subMillisecondTimestampsSurviveAndCompare() throws {
            let anchor = Date(timeIntervalSinceReferenceDate: 800_000_000.123_456_7)
            var before = DatabaseTestSuites.makeFile(ocId: "before", fileName: "before.txt")
            before.downloaded = true
            before.syncTime = Date(timeIntervalSinceReferenceDate: anchor.timeIntervalSinceReferenceDate - 0.000_001)
            var after = DatabaseTestSuites.makeFile(ocId: "after", fileName: "after.txt")
            after.downloaded = true
            after.syncTime = Date(timeIntervalSinceReferenceDate: anchor.timeIntervalSinceReferenceDate + 0.000_001)
            // The first millisecond of the anchor's second: a millisecond text format would truncate this to the anchor itself.
            var firstMillisecond = DatabaseTestSuites.makeFile(ocId: "first-ms", fileName: "first.txt")
            firstMillisecond.downloaded = true
            firstMillisecond.syncTime = Date(timeIntervalSinceReferenceDate: 800_000_001.000_4)

            try manager.insertForTesting(before)
            try manager.insertForTesting(after)
            try manager.insertForTesting(firstMillisecond)

            #expect(manager.itemMetadata(ocId: "after")?.syncTime == after.syncTime)
            #expect(manager.itemMetadata(ocId: "before")?.syncTime == before.syncTime)

            let changes = try #require(manager.pendingWorkingSetChanges(since: anchor))
            #expect(Set(changes.updated.map(\.ocId)) == ["after", "first-ms"])

            let laterChanges = try #require(manager.pendingWorkingSetChanges(since: Date(timeIntervalSinceReferenceDate: 800_000_001)))
            #expect(laterChanges.updated.map(\.ocId) == ["first-ms"])
        }

        @Test func arraysAreStoredAsJSONText() throws {
            var metadata = DatabaseTestSuites.makeFile(ocId: "arrays", fileName: "arrays.txt")
            metadata.shareType = [0, 3]
            metadata.tags = ["x", "y"]
            metadata.sharePermissionsCloudMesh = []

            try manager.insertForTesting(metadata)

            let raw = try manager.writer.read { db in
                try Row.fetchOne(db, sql: "SELECT shareType, tags, sharePermissionsCloudMesh FROM itemMetadata WHERE ocId = ?", arguments: ["arrays"])
            }
            #expect(raw?["shareType"] as String? == "[0,3]")
            #expect(raw?["tags"] as String? == "[\"x\",\"y\"]")
            #expect(raw?["sharePermissionsCloudMesh"] as String? == "[]")
            #expect(manager.itemMetadata(ocId: "arrays")?.shareType == [0, 3])
        }

        @Test func normalizedKeysFollowTheRawLocation() {
            var record = ItemMetadataRecord(DatabaseTestSuites.makeFile(ocId: "nfd", fileName: "re\u{0301}sume\u{0301}.txt", serverUrl: "https://host/pre\u{0302}t"))
            #expect(record.normalizedFileName == "résumé.txt".precomposedStringWithCanonicalMapping)
            #expect(record.normalizedServerUrl == "https://host/prêt".precomposedStringWithCanonicalMapping)

            record.updateLocation(serverUrl: "https://host/plain", fileName: "plain.txt")
            #expect(record.normalizedServerUrl == "https://host/plain")
            #expect(record.normalizedFileName == "plain.txt")
        }

        @Test func upsertKeepsTheRowIdentity() throws {
            var metadata = DatabaseTestSuites.makeFile(ocId: "same", fileName: "same.txt")
            try manager.insertForTesting(metadata)
            let firstRowId = try manager.writer.read { db in try Int64.fetchOne(db, sql: "SELECT rowid FROM itemMetadata WHERE ocId = 'same'") }

            metadata.etag = "changed"
            manager.addItemMetadata(metadata)

            let secondRowId = try manager.writer.read { db in try Int64.fetchOne(db, sql: "SELECT rowid FROM itemMetadata WHERE ocId = 'same'") }
            #expect(firstRowId == secondRowId)
            #expect(manager.itemMetadata(ocId: "same")?.etag == "changed")
        }

        @Test func persistedJSONShapeOfTheValueTypeIsUnchanged() throws {
            let metadata = makeFullMetadata()
            let data = try JSONEncoder().encode(metadata)
            let object = try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])

            // Change-delivery sessions written by earlier builds decode with this key set. Normalized keys are not part of it.
            let expectedKeys: Set = [
                "ocId", "account", "checksums", "chunkUploadId", "classFile", "commentsUnread", "contentType", "creationDate",
                "dataFingerprint", "date", "syncTime", "deleted", "directory", "downloadURL", "e2eEncrypted", "etag",
                "fileProviderContentVersion", "favorite", "fileId", "fileName", "fileNameView", "hasPreview", "hidden", "iconName",
                "iconUrl", "isLockFileOfLocalOrigin", "mountType", "name", "note", "ownerId", "ownerDisplayName", "livePhotoFile",
                "lock", "lockOwner", "lockOwnerEditor", "lockOwnerType", "lockOwnerDisplayName", "lockTime", "lockTimeOut", "lockToken",
                "path", "permissions", "quotaUsedBytes", "quotaAvailableBytes", "resourceType", "richWorkspace", "serverUrl", "session",
                "sessionError", "sessionTaskIdentifier", "sharePermissionsCollaborationServices", "sharePermissionsCloudMesh", "shareType",
                "size", "status", "tags", "downloaded", "uploaded", "keepDownloaded", "visitedDirectory", "trashbinFileName",
                "trashbinOriginalLocation", "trashbinDeletionTime", "uploadDate", "urlBase", "user", "userId"
            ]
            #expect(Set(object.keys) == expectedKeys)
            #expect(expectedKeys.count == 67)
        }
    }
}
