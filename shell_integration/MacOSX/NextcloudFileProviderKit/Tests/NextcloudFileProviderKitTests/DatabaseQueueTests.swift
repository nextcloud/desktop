//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import XCTest

///
/// Guards for the properties of ``FilesDatabaseManager/perform(_:)`` that keep concurrent
/// enumeration from starving the cooperative pool and hanging the extension.
///
final class DatabaseQueueTests: XCTestCase {
    private static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    private func makeDatabaseDirectory() -> URL {
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("DatabaseQueueTests-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory
    }

    /// The work must leave the caller's executor, asserted by asking where the body actually ran
    /// rather than by timing it.
    func testWorkRunsOffTheCallingExecutorAndOnTheDatabaseQueue() async {
        let onDatabaseQueue = await dbManager.perform { $0.isOnDatabaseQueue }

        XCTAssertTrue(
            onDatabaseQueue,
            "Database work must run on the database queue, not on the cooperative pool thread that asked for it."
        )
        XCTAssertFalse(
            dbManager.isOnDatabaseQueue,
            "The caller itself must not be on the database queue; that would mean the hop did not happen."
        )
    }

    /// Serial execution keeps commits from piling up on Realm's write lock, and a peak above one
    /// means two bodies were inside the critical section at once however the scheduler interleaved
    /// them.
    func testConcurrentCallersNeverExecuteAtTheSameTime() async throws {
        let tracker = ConcurrencyTracker()
        let dbManager = try XCTUnwrap(dbManager)

        await withTaskGroup(of: Void.self) { group in
            for _ in 0 ..< 64 {
                group.addTask {
                    await dbManager.perform { _ in
                        // Enter and leave inside the body, which *is* the critical section;
                        // straddling the hop would only count suspended callers.
                        tracker.enter()
                        tracker.doRepresentativeWork()
                        tracker.leave()
                    }
                }
            }
        }

        XCTAssertEqual(
            tracker.peak, 1,
            "Database work must be serialized; overlapping bodies contend for Realm's commit lock."
        )
        XCTAssertEqual(tracker.completed, 64, "Every caller should have run exactly once.")
    }

    /// Nesting is the case that would deadlock if the queue were ever made to run work
    /// synchronously on the caller.
    func testANestedCallCompletesRatherThanDeadlocking() async {
        let inner = await dbManager.perform { manager in manager.isOnDatabaseQueue }
        let outer = await dbManager.perform { _ in inner }

        XCTAssertTrue(outer, "Sequential hops must complete without deadlocking.")
    }
}

/// Records how many bodies were inside the critical section simultaneously, guarded by `NSLock`
/// rather than an actor because the counter is read from the database queue, which must not await.
private final class ConcurrencyTracker: @unchecked Sendable {
    private let lock = NSLock()
    private var current = 0
    private var highWaterMark = 0
    private var finished = 0

    func enter() {
        lock.lock()
        defer { lock.unlock() }
        current += 1
        highWaterMark = max(highWaterMark, current)
    }

    func leave() {
        lock.lock()
        defer { lock.unlock() }
        current -= 1
        finished += 1
    }

    /// Occupy the section long enough that a concurrent queue would reliably be caught, with busy
    /// work rather than a sleep so the passing case does not depend on timing.
    func doRepresentativeWork() {
        var accumulator = 0
        for value in 0 ..< 20000 {
            accumulator &+= value
        }
        precondition(accumulator > 0)
    }

    var peak: Int {
        lock.lock()
        defer { lock.unlock() }
        return highWaterMark
    }

    var completed: Int {
        lock.lock()
        defer { lock.unlock() }
        return finished
    }
}
