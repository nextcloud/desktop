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

            let session = try #require(manager.changeDeliverySession(forAnchorKey: "a0", containerKey: "c"))
            #expect(session.sessionId == "s")
            #expect(session.nextSequence == 0)
            #expect(session.hardRemoveDeleted)
            #expect(manager.changeDeliverySession(forAnchorKey: "a0", containerKey: "other") == nil)
        }

        @Test func acknowledgingWithoutDeletionsLeavesItemMetadataAlone() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "untouched", fileName: "u.txt"))
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 2), deleted: [], incomplete: false, hardRemoveDeleted: true))
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 2, nextAnchorKey: nil, moreComing: false))

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", deletedOcIds: []))

            #expect(manager.itemMetadata(ocId: "untouched")?.deleted == false)
            #expect(manager.changeDeliverySession(sessionId: "s") == nil)
            #expect(try #require(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.isEmpty))
        }

        @Test func acknowledgingAnIntermediateBatchAdvancesTheSessionAndDropsDeliveredItems() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "soft", fileName: "soft.txt"))
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 5), deleted: [], incomplete: false, hardRemoveDeleted: false))
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 2, nextAnchorKey: "a1", moreComing: true))
            // Preparing the same batch again is idempotent; a different one is refused.
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 2, nextAnchorKey: "a1", moreComing: true))
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 3, nextAnchorKey: "a1", moreComing: true) == false)

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", deletedOcIds: ["soft"]))

            let session = try #require(manager.changeDeliverySession(forAnchorKey: "a1", containerKey: "c"))
            #expect(session.nextSequence == 2)
            #expect(session.pendingReported == false)
            #expect(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.map(\.sequence) == [2, 3, 4])
            #expect(manager.itemMetadata(ocId: "soft")?.deleted == true, "Soft removal keeps the tombstone.")
        }

        @Test func acknowledgingAnAcknowledgedBatchAgainChangesNothing() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "soft", fileName: "soft.txt"))
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 5), deleted: [], incomplete: false, hardRemoveDeleted: false))
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 2, nextAnchorKey: "a1", moreComing: true))
            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", deletedOcIds: ["soft"]))

            // The reporting instance and a fresh one asked for the continuation both acknowledge the same batch.
            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", deletedOcIds: ["soft"]))

            let session = try #require(manager.changeDeliverySession(forAnchorKey: "a1", containerKey: "c"))
            #expect(session.nextSequence == 2)
            #expect(session.pendingReported == false)
            #expect(manager.changeDeliveryItems(sessionId: "s", fromSequence: 0, limit: 10)?.map(\.sequence) == [2, 3, 4], "The cursor moved once.")
        }

        @Test func acknowledgingWithHardRemovalDeletesTheRows() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "hard", fileName: "hard.txt"))
            #expect(manager.createChangeDeliverySession(sessionId: "s", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: [], deleted: [DatabaseTestSuites.makeFile(ocId: "hard", fileName: "hard.txt")], incomplete: false, hardRemoveDeleted: true))
            #expect(manager.prepareChangeDeliveryBatch(sessionId: "s", endSequence: 1, nextAnchorKey: nil, moreComing: false))
            #expect(manager.pendingChangeDeliveryDeletedOcIds(sessionId: "s") == ["hard"])

            #expect(manager.acknowledgeChangeDeliveryBatch(sessionId: "s", deletedOcIds: ["hard"]))

            #expect(manager.itemMetadata(ocId: "hard") == nil)
        }

        @Test func removingTheSessionsOfAContainerRemovesTheirItems() throws {
            #expect(manager.createChangeDeliverySession(sessionId: "s1", containerKey: "c", anchorKey: "a0", finalAnchorRawValue: Data(), updated: makeRows(count: 2), deleted: [], incomplete: false, hardRemoveDeleted: false))
            #expect(manager.createChangeDeliverySession(sessionId: "s2", containerKey: "other", anchorKey: "b0", finalAnchorRawValue: Data(), updated: makeRows(count: 1), deleted: [], incomplete: false, hardRemoveDeleted: false))

            manager.removeChangeDeliverySessions(containerKey: "c")

            #expect(manager.changeDeliverySession(sessionId: "s1") == nil)
            #expect(try #require(manager.changeDeliveryItems(sessionId: "s1", fromSequence: 0, limit: 10)?.isEmpty))
            #expect(manager.changeDeliverySession(sessionId: "s2") != nil)
            #expect(manager.changeDeliveryItems(sessionId: "s2", fromSequence: 0, limit: 10)?.count == 1)
        }
    }
}
