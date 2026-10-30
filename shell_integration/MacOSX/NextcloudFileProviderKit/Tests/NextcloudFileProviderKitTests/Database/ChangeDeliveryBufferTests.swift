//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the change-delivery buffer when its stored items cannot be read.
    ///
    @Suite("Change delivery buffer")
    struct ChangeDeliveryBufferTests {
        let manager = DatabaseTestSuites.makeManager()

        private func makeBuffer(hardRemoveDeleted: Bool = false) -> ChangeDeliveryBuffer {
            ChangeDeliveryBuffer(dbManager: manager, containerKey: "container", hardRemoveDeleted: hardRemoveDeleted, log: FileProviderLogMock())
        }

        private var rows: [SendableItemMetadata] {
            (0 ..< 3).map { DatabaseTestSuites.makeFile(ocId: "row-\($0)", fileName: "row-\($0).txt") }
        }

        private func storeDeletedItems(_ ocIds: [String]) -> [SendableItemMetadata] {
            ocIds.map { ocId in
                var metadata = DatabaseTestSuites.makeFile(ocId: ocId, fileName: "\(ocId).txt")
                metadata.deleted = true
                manager.addItemMetadata(metadata)
                return metadata
            }
        }

        private func anchorKey(_ rawValue: Data?) throws -> String {
            try #require(rawValue.flatMap { String(data: $0, encoding: .utf8) })
        }

        @Test func aLateAcknowledgementOfThePreviousBatchDoesNotCommitTheNextBatch() throws {
            let deleted = storeDeletedItems(["d0", "d1", "d2", "d3"])
            let first = makeBuffer(hardRemoveDeleted: true)
            first.prime(key: "initial", finalAnchorRawValue: Data("final".utf8), updated: Array(rows.prefix(1)), deleted: deleted)
            let firstBatch = try #require(first.prepareChangeDeliveryBatch(maxItems: 2))
            let firstAnchor = try anchorKey(firstBatch.continuationAnchorRawValue)

            // The next request arrives on a new enumerator before the first one acknowledged its batch.
            let second = makeBuffer(hardRemoveDeleted: true)
            #expect(try second.isContinuation(forKey: firstAnchor))
            let secondBatch = try #require(second.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(secondBatch.deleted.map(\.ocId) == ["d1", "d2"])
            let secondAnchor = try anchorKey(secondBatch.continuationAnchorRawValue)

            first.acknowledgeBatch(firstBatch.acknowledgement)
            second.acknowledgeBatch(secondBatch.acknowledgement)

            #expect(manager.itemMetadata(ocId: "d0") == nil)
            #expect(manager.itemMetadata(ocId: "d1") == nil, "The second batch is acknowledged with its own deletions.")
            #expect(manager.itemMetadata(ocId: "d2") == nil, "The second batch is acknowledged with its own deletions.")

            let third = makeBuffer(hardRemoveDeleted: true)
            #expect(try third.isContinuation(forKey: secondAnchor))
            let thirdBatch = try #require(third.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(thirdBatch.deleted.map(\.ocId) == ["d3"])
            #expect(thirdBatch.moreComing == false)
        }

        @Test func aLateAcknowledgementFromThePreviousSessionDoesNotCommitTheNextSession() throws {
            let firstFinalAnchor = Data("first-final".utf8)

            // A reused enumerator starts the next session before the previous session's acknowledgement ran.
            let buffer = makeBuffer(hardRemoveDeleted: true)
            buffer.prime(key: "first", finalAnchorRawValue: firstFinalAnchor, updated: [], deleted: storeDeletedItems(["d0"]))
            let firstBatch = try #require(buffer.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(try buffer.isPrimed(forKey: anchorKey(firstFinalAnchor)) == false)
            buffer.reset()
            buffer.prime(key: "second", finalAnchorRawValue: Data("second-final".utf8), updated: [], deleted: storeDeletedItems(["d1"]))
            let secondBatch = try #require(buffer.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(firstBatch.acknowledgement?.endSequence == secondBatch.acknowledgement?.endSequence)

            buffer.acknowledgeBatch(firstBatch.acknowledgement)

            #expect(manager.itemMetadata(ocId: "d1") != nil, "The next session's batch stays unacknowledged.")
            buffer.acknowledgeBatch(secondBatch.acknowledgement)
            #expect(manager.itemMetadata(ocId: "d1") == nil)
        }

        @Test func aPrimedBufferHandsOutBatchesWithAContinuation() throws {
            let buffer = makeBuffer()
            buffer.prime(key: "anchor", finalAnchorRawValue: Data([9]), updated: rows, deleted: [])

            let batch = try #require(buffer.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(batch.updated.map(\.ocId) == ["row-0", "row-1"])
            #expect(batch.moreComing)
            #expect(batch.continuationAnchorRawValue != nil)
        }

        @Test func aFreshBufferContinuesABatchTheReportingBufferAcknowledgedMeanwhile() throws {
            let reporting = makeBuffer()
            reporting.prime(key: "anchor", finalAnchorRawValue: Data([9]), updated: rows, deleted: [])
            let batch = try #require(reporting.prepareChangeDeliveryBatch(maxItems: 2))
            let continuation = try #require(batch.continuationAnchorRawValue.flatMap { String(data: $0, encoding: .utf8) })

            // The observer finished; the framework asks for the continuation through a fresh instance while the reporting one acknowledges.
            reporting.acknowledgeBatch(batch.acknowledgement)
            let fresh = makeBuffer()

            #expect(try fresh.isContinuation(forKey: continuation))
            let next = try #require(fresh.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(next.updated.map(\.ocId) == ["row-2"], "The drain continues where the acknowledged batch ended.")
            #expect(next.moreComing == false)
        }

        @Test func anUnreadableItemTableYieldsNoBatchAndKeepsTheSession() throws {
            let buffer = makeBuffer()
            buffer.prime(key: "anchor", finalAnchorRawValue: Data([9]), updated: rows, deleted: [])
            #expect(try buffer.isPrimed(forKey: "anchor"))

            try manager.breakTableForTesting(ChangeDeliveryItemRecord.databaseTableName)

            #expect(buffer.prepareChangeDeliveryBatch(maxItems: 2) == nil)
            let session = try #require(try manager.changeDeliverySession(forContinuationKey: "anchor", containerKey: "container"))
            #expect(session.nextSequence == 0)
            #expect(session.pendingReported == false, "Nothing was prepared, so nothing can be acknowledged past the undelivered changes.")
        }

        @Test func anUnreadableSessionTableYieldsNoBatchAndThrowsFromThePredicates() throws {
            let buffer = makeBuffer()
            buffer.prime(key: "anchor", finalAnchorRawValue: Data([9]), updated: rows, deleted: [])

            try manager.breakTableForTesting(ChangeDeliverySessionRecord.databaseTableName)

            #expect(buffer.prepareChangeDeliveryBatch(maxItems: 2) == nil, "An empty final batch would acknowledge changes which were never delivered.")
            #expect(throws: (any Error).self) { try buffer.isPrimed(forKey: "anchor") }
            #expect(throws: (any Error).self) { try buffer.isPrimedIncomplete() }
            let fresh = makeBuffer()
            #expect(throws: (any Error).self) { try fresh.isContinuation(forKey: "fp-continuation|x|1") }
            #expect(try fresh.isContinuation(forKey: "anchor") == false, "A plain anchor is answered without the database.")
        }

        @Test func aBufferWhosePrimeFailedYieldsNoBatch() throws {
            try manager.breakTableForTesting(ChangeDeliverySessionRecord.databaseTableName)
            let buffer = makeBuffer()
            buffer.prime(key: "anchor", finalAnchorRawValue: Data([9]), updated: rows, deleted: [])

            #expect(buffer.prepareChangeDeliveryBatch(maxItems: 2) == nil, "The changes were not stored; an empty final batch would drop them.")
        }
    }
}
