//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import XCTest

///
/// Coverage for an enumerator acknowledging its batch only after the framework already asked for the next one,
/// possibly on a new enumerator or in a new session.
///
/// XCTest instead of Swift Testing because the database works through the process-wide Realm default
/// configuration, which concurrently running Swift Testing suites would share.
///
final class ChangeDeliveryBufferTests: NextcloudFileProviderKitTestCase {
    static let account = Account(user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd")
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
    static let containerKey = "working-set"
    static let batchSize = 2

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    private func makeBuffer() -> ChangeDeliveryBuffer {
        ChangeDeliveryBuffer(dbManager: Self.dbManager, containerKey: Self.containerKey, hardRemoveDeleted: true, log: FileProviderLogMock())
    }

    private func storeDeletedItems(_ ocIds: [String]) -> [SendableItemMetadata] {
        ocIds.map { ocId in
            var metadata = SendableItemMetadata(ocId: ocId, fileName: "\(ocId).txt", account: Self.account)
            metadata.deleted = true
            Self.dbManager.addItemMetadata(metadata)
            return metadata
        }
    }

    private func anchorKey(_ rawValue: Data?) throws -> String {
        try XCTUnwrap(rawValue.flatMap { String(data: $0, encoding: .utf8) })
    }

    func testLateAcknowledgementOfPreviousBatchDoesNotCommitTheNextBatch() throws {
        let db = Self.dbManager.ncDatabase() // Strong reference for the in-memory database.
        debugPrint(db)
        let deleted = storeDeletedItems(["d0", "d1", "d2", "d3"])
        let updated = [SendableItemMetadata(ocId: "u0", fileName: "u0.txt", account: Self.account)]

        let first = makeBuffer()
        first.prime(key: "initial", finalAnchorRawValue: Data("final".utf8), updated: updated, deleted: deleted)
        let firstBatch = first.prepareChangeDeliveryBatch(maxItems: Self.batchSize)
        let firstAnchor = try anchorKey(firstBatch.continuationAnchorRawValue)

        // The next request arrives on a new enumerator before the first one acknowledged its batch.
        let second = makeBuffer()
        XCTAssertTrue(second.isContinuation(forKey: firstAnchor))
        let secondBatch = second.prepareChangeDeliveryBatch(maxItems: Self.batchSize)
        XCTAssertEqual(secondBatch.deleted.map(\.ocId), ["d1", "d2"])
        let secondAnchor = try anchorKey(secondBatch.continuationAnchorRawValue)

        first.acknowledgeBatch(firstBatch.acknowledgement)
        second.acknowledgeBatch(secondBatch.acknowledgement)

        XCTAssertNil(Self.dbManager.itemMetadata(ocId: "d0"))
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: "d1"), "The second batch is acknowledged with its own deletions.")
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: "d2"), "The second batch is acknowledged with its own deletions.")

        let third = makeBuffer()
        XCTAssertTrue(third.isContinuation(forKey: secondAnchor))
        let thirdBatch = third.prepareChangeDeliveryBatch(maxItems: Self.batchSize)
        XCTAssertEqual(thirdBatch.deleted.map(\.ocId), ["d3"])
        XCTAssertFalse(thirdBatch.moreComing)
    }

    func testLateAcknowledgementFromPreviousSessionDoesNotCommitTheNextSession() throws {
        let db = Self.dbManager.ncDatabase() // Strong reference for the in-memory database.
        debugPrint(db)
        let firstFinalAnchor = Data("first-final".utf8)

        // A reused enumerator starts the next session before the previous session's acknowledgement ran.
        let buffer = makeBuffer()
        buffer.prime(key: "first", finalAnchorRawValue: firstFinalAnchor, updated: [], deleted: storeDeletedItems(["d0"]))
        let firstBatch = buffer.prepareChangeDeliveryBatch(maxItems: Self.batchSize)
        XCTAssertFalse(try buffer.isPrimed(forKey: anchorKey(firstFinalAnchor)))
        buffer.reset()
        buffer.prime(key: "second", finalAnchorRawValue: Data("second-final".utf8), updated: [], deleted: storeDeletedItems(["d1"]))
        let secondBatch = buffer.prepareChangeDeliveryBatch(maxItems: Self.batchSize)
        XCTAssertEqual(firstBatch.acknowledgement?.endSequence, secondBatch.acknowledgement?.endSequence)

        buffer.acknowledgeBatch(firstBatch.acknowledgement)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: "d1"), "The next session's batch stays unacknowledged.")
        buffer.acknowledgeBatch(secondBatch.acknowledgement)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: "d1"))
    }
}
