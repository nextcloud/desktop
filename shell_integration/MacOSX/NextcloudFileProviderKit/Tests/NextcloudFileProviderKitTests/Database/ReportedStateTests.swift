//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
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

        // MARK: - Derivation

        private let anchor = Date(timeIntervalSinceReferenceDate: 900)

        private func makeDirectory(ocId: String, fileName: String, syncTime: TimeInterval, visited: Bool = true) -> SendableItemMetadata {
            var directory = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName)
            directory.directory = true
            directory.visitedDirectory = visited
            directory.syncTime = Date(timeIntervalSinceReferenceDate: syncTime)
            return directory
        }

        private func makeChild(ocId: String, of directory: SendableItemMetadata, syncTime: TimeInterval, deleted: Bool = false) -> SendableItemMetadata {
            var child = DatabaseTestSuites.makeFile(ocId: ocId, fileName: "\(ocId).txt", serverUrl: directory.serverUrl + "/" + directory.fileName)
            child.deleted = deleted
            child.syncTime = Date(timeIntervalSinceReferenceDate: syncTime)
            return child
        }

        @Test func reportedRowsLeaveThePendingWorkingSetChanges() throws {
            let row = makeRow(syncTime: 1000)
            try manager.insertForTesting(row)
            #expect(manager.pendingWorkingSetChanges(since: anchor)?.updated.map(\.ocId) == ["row"])

            manager.markReported([row])

            #expect(try #require(manager.pendingWorkingSetChanges(since: anchor)).updated.isEmpty)
        }

        @Test func aRowRewrittenAfterItsReportIsPendingAgain() throws {
            let row = makeRow(syncTime: 1000)
            manager.addItemMetadata(row)
            manager.markReported([row])

            var changed = row
            changed.etag = "changed"
            changed.syncTime = Date(timeIntervalSinceReferenceDate: 1001)
            manager.addItemMetadata(changed)
            #expect(manager.pendingWorkingSetChanges(since: anchor)?.updated.map(\.ocId) == ["row"])

            manager.markReported([changed])
            #expect(try #require(manager.pendingWorkingSetChanges(since: anchor)).updated.isEmpty)
        }

        @Test func aRewriteCarryingAnOlderStampStaysPending() throws {
            try manager.insertForTesting(makeRow(syncTime: 1000))
            try manager.insertForTesting(makeRow(syncTime: 1200))

            manager.markReported([makeRow(syncTime: 1000)])

            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == Date(timeIntervalSinceReferenceDate: 1000))
            #expect(manager.pendingWorkingSetChanges(since: anchor)?.updated.map(\.ocId) == ["row"])
        }

        @Test func aMarkedDirectoryIsStillExpanded() throws {
            let directory = makeDirectory(ocId: "dir", fileName: "Dir", syncTime: 1000)
            let reportedChild = makeChild(ocId: "reported", of: directory, syncTime: 1000)
            let pendingChild = makeChild(ocId: "pending", of: directory, syncTime: 1000)
            let tombstone = makeChild(ocId: "gone", of: directory, syncTime: 1000, deleted: true)
            for row in [directory, reportedChild, pendingChild, tombstone] {
                try manager.insertForTesting(row)
            }
            manager.markReported([directory, reportedChild, tombstone])

            let changes = try #require(manager.pendingWorkingSetChanges(since: anchor))

            #expect(changes.updated.map(\.ocId) == ["pending"], "The directory and the reported child are known to the system; the pending child is not.")
            #expect(changes.deleted.map(\.ocId) == ["gone"], "A deletion is reported whether the row was marked or not.")
        }

        @Test func theRootContainerIsExpandedButNotReported() throws {
            var root = makeDirectory(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue, fileName: "", syncTime: 1000)
            root.serverUrl = DatabaseTestSuites.account.davFilesUrl
            try manager.insertForTesting(root)
            try manager.insertForTesting(makeRow(ocId: "child", syncTime: 1000))

            let changes = try #require(manager.pendingWorkingSetChanges(since: anchor))

            #expect(changes.updated.map(\.ocId) == ["child"], "The root row is rewritten by every root listing and never handed over; the system synthesises the root item.")
        }

        @Test func aReportedRowSoftDeletedLaterIsReportedAsDeleted() throws {
            let row = makeRow(syncTime: 1000)
            manager.addItemMetadata(row)
            manager.markReported([row])

            manager.deleteItemMetadata(ocId: "row")

            let changes = try #require(manager.pendingWorkingSetChanges(since: anchor))
            #expect(changes.updated.isEmpty)
            #expect(changes.deleted.map(\.ocId) == ["row"])
        }

        @Test func aListingWhichChangedNothingKeepsTheRowReported() throws {
            let row = makeRow(syncTime: 1000)
            manager.addItemMetadata(row)
            manager.markReported([row])

            var relisted = row
            relisted.syncTime = Date(timeIntervalSinceReferenceDate: 1100)
            manager.addItemMetadataPreservingLocalState(relisted)

            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == row.syncTime, "A listing which changes nothing does not rewrite the row, so the mark still covers it.")
            #expect(try #require(manager.pendingWorkingSetChanges(since: anchor)).updated.isEmpty)

            var changed = relisted
            changed.etag = "changed"
            changed.syncTime = Date(timeIntervalSinceReferenceDate: 1200)
            manager.addItemMetadataPreservingLocalState(changed)

            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == row.syncTime, "A remote change keeps the previous mark.")
            #expect(manager.pendingWorkingSetChanges(since: anchor)?.updated.map(\.ocId) == ["row"])
        }

        ///
        /// A visit is local state, so recording it rewrites a row whose remote state the system already has.
        ///
        /// The row is stamped anew by that write, and the mark has to follow, or the directory comes back as
        /// a working-set change for a visit the system itself asked for.
        ///
        @Test func recordingAVisitKeepsAnUnchangedRowReported() throws {
            var directory = makeRow(syncTime: 1000)
            directory.directory = true
            directory.downloaded = false
            manager.addItemMetadata(directory)
            manager.markReported([directory])

            var visited = directory
            visited.visitedDirectory = true
            visited.syncTime = Date(timeIntervalSinceReferenceDate: 1100)
            manager.addItemMetadataPreservingLocalState(visited, preserveVisitedDirectory: false)

            #expect(manager.itemMetadata(ocId: "row")?.visitedDirectory == true)
            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == visited.syncTime, "The mark follows the rewrite because the system has this remote state.")
            #expect(try #require(manager.pendingWorkingSetChanges(since: anchor)).updated.isEmpty)
        }

        @Test func aListingOfAnUnreportedRowDoesNotMarkIt() {
            let row = makeRow(syncTime: 1000)
            manager.addItemMetadata(row)

            var relisted = row
            relisted.syncTime = Date(timeIntervalSinceReferenceDate: 1100)
            manager.addItemMetadataPreservingLocalState(relisted)

            #expect(manager.reportedSyncTimeForTesting(ocId: "row") == nil)
            #expect(manager.pendingWorkingSetChanges(since: anchor)?.updated.map(\.ocId) == ["row"])
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
