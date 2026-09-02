//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import XCTest

///
/// Guards for the properties of ``FilesDatabaseManager/perform(_:)`` that keep concurrent
/// enumeration from starving the cooperative pool and hanging the extension.
///
final class DatabasePerformTests: NextcloudFileProviderKitTestCase {
    private static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!

    override func setUpWithError() throws {
        try super.setUpWithError()
        dbManager = try FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    /// The work must leave the caller's executor, asserted by asking where the body actually ran
    /// rather than by timing it.
    func testWorkRunsOffTheCallingExecutorAndOnTheBlockingCallQueue() async {
        let onBlockingCallQueue = await dbManager.perform { $0.isOnBlockingCallQueue }

        XCTAssertTrue(
            onBlockingCallQueue,
            "Database work must run on the blocking-call queue, not on the cooperative pool thread that asked for it."
        )
        XCTAssertFalse(
            dbManager.isOnBlockingCallQueue,
            "The caller itself must not be on the blocking-call queue; that would mean the hop did not happen."
        )
    }

    /// The first body waits for a signal only the second body sends, so a queue that ran them one
    /// after the other would let the wait time out.
    func testConcurrentCallersAreNotSerialised() async throws {
        let dbManager = try XCTUnwrap(dbManager)
        let secondStarted = DispatchSemaphore(value: 0)

        async let firstSawSecond = dbManager.perform { _ in
            secondStarted.wait(timeout: .now() + 5) == .success
        }
        async let second: Void = dbManager.perform { _ in
            secondStarted.signal()
        }

        let (overlapped, _) = await (firstSawSecond, second)

        XCTAssertTrue(
            overlapped,
            "Callers must not be serialised by the hop; the pool already serialises writes and runs reads in parallel."
        )
    }

    /// Concurrent hops rely on the pool to serialise writes, so none of them may be lost.
    func testConcurrentWritesAreAllPersisted() async throws {
        let dbManager = try XCTUnwrap(dbManager)
        let ocIds = (0 ..< 64).map { "concurrent-\($0)" }

        await withTaskGroup(of: Void.self) { group in
            for ocId in ocIds {
                group.addTask {
                    await dbManager.perform { manager in
                        manager.addItemMetadata(SendableItemMetadata(ocId: ocId, fileName: "\(ocId).txt", account: Self.account))
                    }
                }
            }
        }

        let stored = await dbManager.perform { manager in
            ocIds.filter { manager.itemMetadata(ocId: $0) != nil }.count
        }

        XCTAssertEqual(stored, ocIds.count, "Every concurrent write must be persisted.")
    }

    /// A body calls methods which each enter the database on their own, so sequential hops that
    /// read what the previous one wrote must complete rather than deadlock.
    func testSequentialHopsSeeEachOthersWrites() async {
        await dbManager.perform { manager in
            manager.addItemMetadata(SendableItemMetadata(ocId: "sequential", fileName: "sequential.txt", account: Self.account))
        }

        let fileName = await dbManager.perform { $0.itemMetadata(ocId: "sequential")?.fileName }

        XCTAssertEqual(fileName, "sequential.txt")
    }
}
