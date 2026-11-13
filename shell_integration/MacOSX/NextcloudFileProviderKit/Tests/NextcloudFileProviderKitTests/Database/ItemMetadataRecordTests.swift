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
            #expect(try ItemMetadataRecord(metadata, domainAccount: manager.account).metadata == metadata)
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

        @Test func normalizedKeysFollowTheRawLocation() throws {
            let account = DatabaseTestSuites.account
            let metadata = DatabaseTestSuites.makeFile(
                ocId: "nfd",
                fileName: "re\u{0301}sume\u{0301}.txt",
                serverUrl: account.davFilesUrl + "/pre\u{0302}t"
            )
            var record = try ItemMetadataRecord(metadata, domainAccount: account)
            #expect(record.normalizedFileName == "résumé.txt".precomposedStringWithCanonicalMapping)
            #expect(record.normalizedParentPath == "/prêt".precomposedStringWithCanonicalMapping)

            try record.updateLocation(parentURL: account.davFilesUrl + "/plain", fileName: "plain.txt", domainAccount: account)
            #expect(record.normalizedParentPath == "/plain")
            #expect(record.normalizedFileName == "plain.txt")
        }

        @Test func filesAndTrashCanContainTheSameParentPathAndFileName() throws {
            let account = DatabaseTestSuites.account
            let folderName = "Team.d1700000000"
            var filesFolder = DatabaseTestSuites.makeFile(
                ocId: "files-folder",
                fileName: folderName,
                serverUrl: account.davFilesUrl
            )
            filesFolder.directory = true
            var trashFolder = DatabaseTestSuites.makeFile(
                ocId: "trash-folder",
                fileName: folderName,
                serverUrl: account.trashUrl
            )
            trashFolder.directory = true
            let filesItem = DatabaseTestSuites.makeFile(
                ocId: "files-report",
                fileName: "Report.txt",
                serverUrl: filesFolder.remotePath()
            )
            let trashItem = DatabaseTestSuites.makeFile(
                ocId: "trash-report",
                fileName: filesItem.fileName,
                serverUrl: trashFolder.remotePath()
            )

            for item in [filesFolder, trashFolder, filesItem, trashItem] {
                manager.addItemMetadata(item)
            }
            #expect(manager.repairPersistedLogicalAddresses().evicted == 0)

            let filesParent = try #require(try manager.storedParentLocationKeysForTesting(ocId: filesItem.ocId))
            let trashParent = try #require(try manager.storedParentLocationKeysForTesting(ocId: trashItem.ocId))
            #expect(filesParent.davRoot == .files)
            #expect(trashParent.davRoot == .trash)
            #expect(filesParent.parentPath == "/" + folderName)
            #expect(trashParent.parentPath == filesParent.parentPath)

            let savedFilesItem = try #require(manager.itemMetadata(locatedAtRemoteUrl: filesItem.remotePath()))
            let savedTrashItem = try #require(manager.itemMetadata(locatedAtRemoteUrl: trashItem.remotePath()))
            #expect(savedFilesItem.ocId == filesItem.ocId)
            #expect(savedTrashItem.ocId == trashItem.ocId)
            #expect(!savedFilesItem.deleted)
            #expect(!savedTrashItem.deleted)
            #expect(manager.immediateChildItems(directoryMetadata: filesFolder).map(\.ocId) == [filesItem.ocId])
            #expect(manager.childItems(directoryMetadata: trashFolder).map(\.ocId) == [trashItem.ocId])
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

        @Test func persistedJSONOmitsOnlyTheRedundantPath() throws {
            let metadata = makeFullMetadata()
            let data = try JSONEncoder().encode(metadata)
            let object = try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])

            // Earlier builds stored this set of keys in change-delivery snapshots.
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
            #expect(Set(object.keys) == expectedKeys.subtracting(["path"]))
        }
    }
}
