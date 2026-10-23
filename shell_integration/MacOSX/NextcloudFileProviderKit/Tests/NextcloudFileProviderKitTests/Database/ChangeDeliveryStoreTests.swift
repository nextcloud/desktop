//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the durable change-delivery sessions and their ordered items.
    ///
    @Suite("Change delivery store")
    struct ChangeDeliveryStoreTests {
        let manager = DatabaseTestSuites.makeManager()

        private func makeRows(count: Int) -> [SendableItemMetadata] {
            (0 ..< count).map { DatabaseTestSuites.makeFile(ocId: "row-\($0)", fileName: "row-\($0).txt") }
        }

        /// Prepare the next batch with continuation keys of the form `a<end sequence>`, as the buffer does with its prefix.
        private func prepare(_ sessionId: String, maxItems: Int) throws -> (items: [(metadataData: Data, deleted: Bool)], endSequence: Int, nextAnchorKey: String?, moreComing: Bool)? {
            try manager.prepareChangeDeliveryBatch(sessionId: sessionId, maxItems: maxItems) { "a\($0)" }
        }

        @Test func itemsAreReturnedInSequenceOrderUpToTheLimit() throws {
            let updated = makeRows(count: 4)
            let deleted = [DatabaseTestSuites.makeFile(ocId: "gone", fileName: "gone.txt")]
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data([1]), updated: updated, deleted: deleted, incomplete: false, hardRemoveDeleted: true))

            let page = try #require(manager.changeDeliveryItems(sessionId: "s", fromSequence: 1, limit: 3))
            #expect(page.map(\.sequence) == [1, 2, 3])
            #expect(page.map(\.deleted) == [false, false, false])

            let tail = try #require(manager.changeDeliveryItems(sessionId: "s", fromSequence: 4, limit: 10))
            #expect(tail.map(\.sequence) == [4])
            #expect(tail.first?.deleted == true)
            #expect(try JSONDecoder().decode(SendableItemMetadata.self, from: #require(tail.first?.metadataData)).ocId == "gone")

            let session = try #require(try manager.changeDeliverySession(forContinuationKey: "a0", containerKey: "c"))
            #expect(session.sessionId == "s")
            #expect(session.nextSequence == 0)
            #expect(session.hardRemoveDeleted)
            #expect(try manager.changeDeliverySession(forContinuationKey: "a0", containerKey: "other") == nil)
        }

        @Test func acknowledgingWithoutDeletionsLeavesItemMetadataAlone() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "untouched", fileName: "u.txt"))
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 2), deleted: [], incomplete: false, hardRemoveDeleted: true))
            #expect(try prepare("s", maxItems: 2)?.endSequence == 2)

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: 2))

            #expect(manager.itemMetadata(ocId: "untouched")?.deleted == false)
            #expect(try manager.changeDeliverySession(sessionId: "s") == nil)
            #expect(try #require(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.isEmpty))
        }

        @Test func acknowledgingAnIntermediateBatchAdvancesTheSessionAndDropsDeliveredItems() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "soft", fileName: "soft.txt"))
            let deleted = ["soft", "later"].map { DatabaseTestSuites.makeFile(ocId: $0, fileName: "\($0).txt") }
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: [], deleted: deleted, incomplete: false, hardRemoveDeleted: false))
            let batch = try #require(try prepare("s", maxItems: 1))
            #expect(batch.endSequence == 1)
            #expect(batch.nextAnchorKey == "a1")

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: 1))

            let session = try #require(try manager.changeDeliverySession(forContinuationKey: "a1", containerKey: "c"))
            #expect(session.nextSequence == 1)
            #expect(session.pendingReported == false)
            #expect(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.map(\.sequence) == [1])
            #expect(manager.itemMetadata(ocId: "soft")?.deleted == true, "Soft removal keeps the tombstone.")
        }

        @Test func acknowledgingAnAcknowledgedBatchAgainChangesNothing() throws {
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 5), deleted: [], incomplete: false, hardRemoveDeleted: false))
            #expect(try prepare("s", maxItems: 2)?.endSequence == 2)
            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: 2))

            // The reporting instance and a fresh one asked for the continuation both acknowledge the same batch.
            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: 2))

            let session = try #require(try manager.changeDeliverySession(forContinuationKey: "a2", containerKey: "c"))
            #expect(session.nextSequence == 2)
            #expect(session.pendingReported == false)
            #expect(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.map(\.sequence) == [2, 3, 4], "The cursor moved once.")
        }

        @Test func acknowledgingWithHardRemovalDeletesTheRows() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "hard", fileName: "hard.txt"))
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: [], deleted: [DatabaseTestSuites.makeFile(ocId: "hard", fileName: "hard.txt")], incomplete: false, hardRemoveDeleted: true))
            let batch = try #require(try prepare("s", maxItems: 1))
            #expect(batch.items.map(\.deleted) == [true])

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: batch.endSequence))

            #expect(manager.itemMetadata(ocId: "hard") == nil)
        }

        @Test func theContinuationKeyFindsTheReportedAndThenTheAcknowledgedBatch() throws {
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 3), deleted: [], incomplete: false, hardRemoveDeleted: true))
            let batch = try #require(try prepare("s", maxItems: 1))
            let anchorKey = try #require(batch.nextAnchorKey)

            let reported = try #require(try manager.changeDeliverySession(forContinuationKey: anchorKey, containerKey: "c"))
            #expect(reported.pendingAnchorKey == anchorKey)
            #expect(reported.currentAnchorKey != anchorKey)

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: batch.endSequence))

            let acknowledged = try #require(try manager.changeDeliverySession(forContinuationKey: anchorKey, containerKey: "c"))
            #expect(acknowledged.currentAnchorKey == anchorKey)
            #expect(acknowledged.pendingReported == false)
        }

        @Test func aStaleAcknowledgementLeavesTheNewerBatchReported() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "d0", fileName: "d0.txt"))
            let deleted = ["d0", "d1"].map { DatabaseTestSuites.makeFile(ocId: $0, fileName: "\($0).txt") }
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 1), deleted: deleted, incomplete: false, hardRemoveDeleted: true))
            let first = try #require(try prepare("s", maxItems: 1))
            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: first.endSequence))
            let second = try #require(try prepare("s", maxItems: 1))

            // The enumerator which reported the first batch acknowledges it again, late.
            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", endSequence: first.endSequence))

            let session = try #require(try manager.changeDeliverySession(sessionId: "s"))
            #expect(session.pendingReported)
            #expect(session.pendingEndSequence == second.endSequence)
            #expect(session.currentAnchorKey == first.nextAnchorKey)
            #expect(manager.itemMetadata(ocId: "d0") != nil, "The deletion of the unacknowledged batch is kept.")
        }

        @Test func preparingAgainReplaysTheReportedBatch() throws {
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 3), deleted: [], incomplete: false, hardRemoveDeleted: true))
            let first = try #require(try prepare("s", maxItems: 2))

            let replayed = try #require(try prepare("s", maxItems: 1))

            #expect(replayed.endSequence == first.endSequence)
            #expect(replayed.nextAnchorKey == first.nextAnchorKey)
            #expect(replayed.items.map(\.metadataData) == first.items.map(\.metadataData))
        }

        @Test func removingTheSessionsOfAContainerRemovesTheirItems() throws {
            #expect(manager.createChangeDeliverySession(sessionId: "s1", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 2), deleted: [], incomplete: false, hardRemoveDeleted: false))
            #expect(manager.createChangeDeliverySession(sessionId: "s2", containerKey: "other", anchorKey: "b0", finalAnchorRawValue: Data(), updated: makeRows(count: 1), deleted: [], incomplete: false, hardRemoveDeleted: false))

            manager.removeChangeDeliverySessions(containerKey: "c")

            #expect(try manager.changeDeliverySession(sessionId: "s1") == nil)
            #expect(try #require(manager.changeDeliveryItems(sessionId: "s1", fromSequence: 0, limit: 10)?.isEmpty))
            #expect(try manager.changeDeliverySession(sessionId: "s2") != nil)
            #expect(manager.changeDeliveryItems(sessionId: "s2", fromSequence: 0, limit: 10)?.count == 1)
        }
    }
}
