//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for what the manager answers when the database cannot be read: lookups which guard a server-side action must fail closed.
    ///
    @Suite("Read failures")
    struct FilesDatabaseManagerFailureTests {
        let manager = DatabaseTestSuites.makeManager()

        @Test func theExclusionLookupThrowsInsteadOfAnsweringNotExcluded() throws {
            manager.markItemAsExcludedFromSync(ocId: "excluded")
            #expect(try manager.excludedFromSyncMarkerExists(ocId: "excluded"))

            try manager.breakTableForTesting(ExcludedFromSyncItemRecord.databaseTableName)

            #expect(throws: (any Error).self) {
                try manager.excludedFromSyncMarkerExists(ocId: "excluded")
            }
            #expect(manager.isItemExcludedFromSync(ocId: "excluded") == false, "The non-throwing form keeps answering false.")
        }

        @Test func chunksAreAssumedToRemainWhenTheLookupFails() throws {
            manager.addRemoteFileChunks([RemoteFileChunk(fileName: "1", size: 1, remoteChunkStoreFolderName: "u")])
            #expect(manager.hasRemoteFileChunks(uploadId: "other") == false)

            try manager.breakTableForTesting(RemoteFileChunkRecord.databaseTableName)

            #expect(manager.hasRemoteFileChunks(uploadId: "other"), "A failed lookup must not discard local chunks.")
        }

        @Test func changeDeliveryReadsReportFailureAsNil() throws {
            let rows = [DatabaseTestSuites.makeFile(ocId: "a", fileName: "a.txt"), DatabaseTestSuites.makeFile(ocId: "b", fileName: "b.txt")]
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: rows, deleted: [], incomplete: false, hardRemoveDeleted: false))
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 1, nextAnchorKey: "a1", moreComing: true))
            #expect(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.count == 2)
            #expect(manager.pendingChangeDeliveryDeletedOcIds(sessionId: "s") == [])

            try manager.breakTableForTesting(ChangeDeliveryItemRecord.databaseTableName)

            #expect(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10) == nil, "An empty array would be taken for the last batch.")
            #expect(manager.pendingChangeDeliveryDeletedOcIds(sessionId: "s") == nil)
            #expect(manager.changeDeliverySession(sessionId: "s") != nil, "The session itself is still there.")
        }

        @Test func theAbandonedUploadDecisionIsNilWhenAnyOfItsQueriesFails() throws {
            var resumable = DatabaseTestSuites.makeFile(ocId: "resumable", fileName: "r.bin")
            resumable.chunkUploadId = "u-resumable"
            resumable.status = Status.uploading.rawValue
            manager.addItemMetadata(resumable)
            manager.addRemoteFileChunks([RemoteFileChunk(fileName: "1", size: 1, remoteChunkStoreFolderName: "u-resumable"), RemoteFileChunk(fileName: "1", size: 1, remoteChunkStoreFolderName: "u-orphan")])
            #expect(manager.abandonedChunkUploadIdentifiers() == ["u-orphan"])

            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(manager.abandonedChunkUploadIdentifiers() == nil, "Chunk rows alone must not turn every upload into an abandoned one.")
        }

        @Test func aDamagedListColumnDecodesAsEmptyAndKeepsTheRow() throws {
            var row = DatabaseTestSuites.makeFile(ocId: "tagged", fileName: "tagged.txt")
            row.tags = ["keep"]
            row.shareType = [3]
            row.downloaded = true
            manager.addItemMetadata(row)
            try manager.writer.write { db in
                try db.execute(sql: "UPDATE itemMetadata SET tags = '[1]', shareType = 'not json' WHERE ocId = 'tagged'")
            }

            let stored = try #require(manager.itemMetadata(ocId: "tagged"))
            #expect(stored.tags == [])
            #expect(stored.shareType == [])
            #expect(stored.sharePermissionsCloudMesh == [])
            #expect(stored.fileName == "tagged.txt")
            #expect(manager.materialisedItemMetadatas(account: "")?.map(\.ocId) == ["tagged"])
        }

        @Test func aMistypedScalarReadsAsItsZeroValueAndKeepsTheResult() throws {
            var healthy = DatabaseTestSuites.makeFile(ocId: "healthy", fileName: "healthy.txt")
            healthy.downloaded = true
            var damaged = DatabaseTestSuites.makeFile(ocId: "damaged", fileName: "damaged.txt")
            damaged.downloaded = true
            manager.addItemMetadata(healthy)
            manager.addItemMetadata(damaged)
            try manager.writer.write { db in
                try db.execute(sql: "UPDATE itemMetadata SET creationDate = 'yesterday', size = 'big' WHERE ocId = 'damaged'")
            }

            // SQLite coerces a mistyped value to the column's zero value, so the row stays readable and the result complete.
            #expect(manager.materialisedItemMetadatas(account: "")?.map(\.ocId) == ["healthy", "damaged"])
            let stored = try #require(manager.itemMetadata(ocId: "damaged"))
            #expect(stored.creationDate == Date(timeIntervalSinceReferenceDate: 0))
            #expect(stored.size == 0)
            #expect(stored.fileName == "damaged.txt")
            #expect(manager.repairPersistedLogicalAddresses() == (0, 0))

            manager.addItemMetadata(damaged)
            #expect(manager.itemMetadata(ocId: "damaged")?.creationDate == damaged.creationDate)
        }

        @Test func materialisedItemsReportFailureAsNil() throws {
            #expect(manager.materialisedItemMetadatas(account: "") == [])

            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(manager.materialisedItemMetadatas(account: "") == nil)
        }

        @Test func pendingWorkingSetChangesReportFailureAsNil() throws {
            var row = DatabaseTestSuites.makeFile(ocId: "changed", fileName: "changed.txt")
            row.downloaded = true
            manager.addItemMetadata(row)
            #expect(manager.pendingWorkingSetChanges(since: Date(timeIntervalSince1970: 0))?.updated.count == 1)

            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(manager.pendingWorkingSetChanges(since: Date(timeIntervalSince1970: 0)) == nil)
            #expect(manager.itemMetadata(ocId: "changed") == nil)
            #expect(manager.itemMetadatas(account: DatabaseTestSuites.account.ncKitAccount).isEmpty)
        }
    }
}
