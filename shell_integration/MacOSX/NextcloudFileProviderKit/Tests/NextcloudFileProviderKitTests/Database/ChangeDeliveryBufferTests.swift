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

        private func makeBuffer() -> ChangeDeliveryBuffer {
            ChangeDeliveryBuffer(dbManager: manager, containerKey: "container", hardRemoveDeleted: false, log: FileProviderLogMock())
        }

        private var rows: [SendableItemMetadata] {
            (0 ..< 3).map { DatabaseTestSuites.makeFile(ocId: "row-\($0)", fileName: "row-\($0).txt") }
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
            reporting.acknowledgeBatch(deletedOcIds: [])
            let fresh = makeBuffer()

            #expect(fresh.isContinuation(forKey: continuation))
            let next = try #require(fresh.prepareChangeDeliveryBatch(maxItems: 2))
            #expect(next.updated.map(\.ocId) == ["row-2"], "The drain continues where the acknowledged batch ended.")
            #expect(next.moreComing == false)
        }

        @Test func anUnreadableItemTableYieldsNoBatchAndKeepsTheSession() throws {
            let buffer = makeBuffer()
            buffer.prime(key: "anchor", finalAnchorRawValue: Data([9]), updated: rows, deleted: [])
            #expect(buffer.isPrimed(forKey: "anchor"))

            try manager.breakTableForTesting(ChangeDeliveryItemRecord.databaseTableName)

            #expect(buffer.prepareChangeDeliveryBatch(maxItems: 2) == nil)
            let session = try #require(manager.changeDeliverySession(forAnchorKey: "anchor", containerKey: "container"))
            #expect(session.nextSequence == 0)
            #expect(session.pendingReported == false, "Nothing was prepared, so nothing can be acknowledged past the undelivered changes.")
        }
    }
}
