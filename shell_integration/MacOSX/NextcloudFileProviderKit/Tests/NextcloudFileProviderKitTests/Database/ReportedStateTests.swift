//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import NextcloudKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the mark which records the row state the system has received.
    ///
    @Suite("Reported state")
    struct ReportedStateTests {
        let manager = DatabaseTestSuites.makeManager()

        private func makeRow(ocId: String = "row", syncTime: TimeInterval) -> SendableItemMetadata {
            var metadata = DatabaseTestSuites.makeFile(ocId: ocId, fileName: "\(ocId).txt")
            metadata.downloaded = true
            metadata.syncTime = Date(timeIntervalSinceReferenceDate: syncTime)
            return metadata
        }

        private func synchronousLevel() throws -> Int? {
            try manager.writer.writeWithoutTransaction { db in try Int.fetchOne(db, sql: "PRAGMA synchronous") }
        }

        @Test func markingRecordsTheHandedOverSyncTime() throws {
            let row = makeRow(syncTime: 1000.5)
            try manager.insertForTesting(row)
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == nil)

            manager.markReported([row])

            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == row.syncTime)
        }

        @Test func markingNeverExceedsTheStoredSyncTime() throws {
            try manager.insertForTesting(makeRow(syncTime: 1000))

            manager.markReported([makeRow(syncTime: 1200)])

            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == Date(timeIntervalSinceReferenceDate: 1000), "A value built before the row was stored must not mark state the system never received.")
        }

        @Test func markingOnlyRaisesTheMark() throws {
            try manager.insertForTesting(makeRow(syncTime: 1000))
            manager.markReported([makeRow(syncTime: 1000)])

            manager.markReported([makeRow(syncTime: 500)])
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == Date(timeIntervalSinceReferenceDate: 1000))

            try manager.insertForTesting(makeRow(syncTime: 1200))
            manager.markReported([makeRow(syncTime: 1200)])
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == Date(timeIntervalSinceReferenceDate: 1200))
        }

        @Test func markingAnUnknownItemOrNothingWritesNothing() throws {
            manager.addItemMetadata(makeRow(ocId: "known", syncTime: 1000))
            #expect(try synchronousLevel() == 2)

            manager.markReported([])
            #expect(try synchronousLevel() == 2, "An empty list opens no transaction.")

            manager.markReported([makeRow(ocId: "unknown", syncTime: 1000)])

            #expect(manager.itemMetadata(ocId: "unknown") == nil)
            #expect(manager.reportedSyncTimeForTesting(ocId: "unknown") == nil)
            #expect(manager.allItemMetadatasForTesting().count == 1)
        }

        @Test func markingCommitsWithRelaxedDurability() throws {
            let row = makeRow(syncTime: 1000)
            manager.addItemMetadata(row)
            #expect(try synchronousLevel() == 2)

            manager.markReported([row])

            #expect(try synchronousLevel() == 1, "A lost mark costs one redundant report, so the write is relaxed.")
        }

        @Test func everyRowRewriteKeepsTheMark() throws {
            let row = makeRow(syncTime: 1000)
            manager.addItemMetadata(row)
            manager.markReported([row])
            let mark = Date(timeIntervalSinceReferenceDate: 1000)

            var changed = row
            changed.etag = "changed"
            changed.syncTime = Date(timeIntervalSinceReferenceDate: 1100)
            manager.addItemMetadata(changed)
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == mark, "The full-row upsert leaves the column alone.")

            manager.addItemMetadataPreservingLocalState(changed)
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == mark)

            manager.renameItemMetadata(ocId: "row", newServerUrl: changed.serverUrl, newFileName: "renamed.txt")
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == mark)

            let lock = NKLock(owner: "alice", ownerEditor: "text", ownerType: .app, ownerDisplayName: "Alice", time: Date(), timeOut: nil, token: "token")
            #expect(try manager.applyLock(lock, rawServerUrl: changed.serverUrl, rawFileName: "renamed.txt"))
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == mark)

            _ = try manager.set(keepDownloaded: true, for: changed)
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == mark)

            manager.deleteItemMetadata(ocId: "row")
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == mark)
        }

        @Test func theDeliveryFilterDropsSnapshotsOlderThanTheMark() throws {
            try manager.insertForTesting(makeRow(ocId: "marked", syncTime: 1000))
            manager.markReported([makeRow(ocId: "marked", syncTime: 1000)])
            try manager.insertForTesting(makeRow(ocId: "unmarked", syncTime: 1000))

            let kept = manager.notYetReported([
                makeRow(ocId: "marked", syncTime: 500),
                makeRow(ocId: "marked", syncTime: 1000),
                makeRow(ocId: "marked", syncTime: 1100),
                makeRow(ocId: "unmarked", syncTime: 500),
                makeRow(ocId: "unknown", syncTime: 500)
            ])

            #expect(kept.map(\.ocId) == ["marked", "marked", "unmarked", "unknown"])
            #expect(kept.map(\.syncTime.timeIntervalSinceReferenceDate) == [1000, 1100, 500, 500], "Only the snapshot older than the mark is dropped; an equal stamp is delivered.")
            #expect(manager.notYetReported([]).isEmpty)
        }

        @Test func theDeliveryFilterHandlesMoreThanOneChunkOfIdentifiers() throws {
            let count = FilesDatabaseManager.inClauseChunkSize + 7
            for index in 0 ..< count {
                let row = makeRow(ocId: "row-\(index)", syncTime: 1000)
                try manager.insertForTesting(row)
                manager.markReported([row])
            }

            let kept = manager.notYetReported((0 ..< count).map { makeRow(ocId: "row-\($0)", syncTime: $0 % 2 == 0 ? 999 : 1000) })

            #expect(kept.count == count / 2, "Every even row carries an older stamp than its mark.")
        }
    }
}
