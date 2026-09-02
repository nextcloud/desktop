//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import XCTest

///
/// Guards for the property that keeps concurrent enumeration from hanging the extension.
///
/// Every Realm access blocks: refresh waits on the coordinator mutex, and a commit walks the
/// reachable object graph before it returns. Running one inside a `Task` parks a thread of the
/// Swift cooperative pool, and the runtime cannot reclaim a thread parked in a mutex. Once enough
/// reads run at once the pool has no threads left, so unrelated work — the enumeration for a folder
/// the user has just opened — never starts. The symptom is a folder that spins forever while the
/// extension sits at full CPU, and it leaves no trace, because the starved task never reaches its
/// own first log line.
///
/// ``FilesDatabaseManager/perform(_:)`` exists to hold two properties that prevent that. Both are
/// invisible when broken and easy to undo by moving a database call back out of the hop, so they
/// are asserted here rather than left to review.
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

    /// The whole point of the hop: the work must leave the caller's executor. Asserted by asking
    /// where the body actually ran, so it holds regardless of how busy the machine is — a timing
    /// assertion would be both flaky and unable to tell a slow pool from a blocked one.
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

    /// Serial execution is what keeps commits from piling up on Realm's write lock. Concurrent
    /// writers gain nothing — Realm serializes commits internally — so overlapping callers only
    /// lengthen the stall that starves the pool.
    ///
    /// The counter makes this deterministic: a peak above one means two bodies were inside at once,
    /// however briefly, no matter how the scheduler happened to interleave them.
    func testConcurrentCallersNeverExecuteAtTheSameTime() async throws {
        let tracker = ConcurrencyTracker()
        let dbManager = try XCTUnwrap(dbManager)

        await withTaskGroup(of: Void.self) { group in
            for _ in 0 ..< 64 {
                group.addTask {
                    await dbManager.perform { _ in
                        // Enter and leave inside the body: the body *is* the critical section.
                        // Straddling the hop would instead measure how many callers are suspended,
                        // which is expected to be all of them.
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

    /// A hop that can deadlock is worse than no hop. Nesting is the case that would do it if the
    /// queue were ever made to run work synchronously on the caller.
    func testANestedCallCompletesRatherThanDeadlocking() async {
        let inner = await dbManager.perform { manager in manager.isOnDatabaseQueue }
        let outer = await dbManager.perform { _ in inner }

        XCTAssertTrue(outer, "Sequential hops must complete without deadlocking.")
    }
}

/// Records how many bodies were inside the critical section simultaneously. `NSLock` rather than an
/// actor: the counter is read from inside the database queue, which must not await.
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

    /// Occupy the section long enough that a concurrent queue would reliably be caught. Busy work
    /// rather than a sleep: it keeps the failing case dependable without making the passing case
    /// depend on timing, since a serial queue cannot overlap however long the body takes.
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
