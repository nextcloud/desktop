//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
@testable import TestInterface
import XCTest

///
/// Streaming change delivery: a working-set scan hands its discoveries over wave by wave rather than
/// holding them until the walk ends, so a change found in the scan's first second is not reported
/// only when its last second completes.
///
/// ## The failure this suite exists for
///
/// A first attempt at streaming was reverted after it silently stopped delivering remote changes.
/// The framework served an intermediate batch from a **second `Enumerator`** — which is the entire
/// reason the session is persisted in Realm — and that instance had no in-memory record of the
/// producer still running. It reported `moreComing: false`, which both ended the sequence and
/// deleted the session out from under the live scan. Nothing was ever reported again, because
/// `moreComing: false` is precisely the statement that the client is synced.
///
/// The original tests all drove a single buffer instance, the one topology where that bug cannot
/// appear. ``testASecondEnumeratorDoesNotEndASequenceWhoseProducerIsStillRunning`` is written first
/// here for that reason: producer liveness has to be durable, not in-memory.
///
final class ChangeDeliveryStreamingTests: NextcloudFileProviderKitTestCase {
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

    /// A buffer as a *different* enumerator would see it: same database, no shared memory.
    private func makeBuffer() -> ChangeDeliveryBuffer {
        ChangeDeliveryBuffer(dbManager: dbManager, log: FileProviderLogMock())
    }

    private func metadata(_ ocId: String) -> SendableItemMetadata {
        SendableItemMetadata(ocId: ocId, fileName: "\(ocId).txt", account: Self.account)
    }

    private func anchorData(_ value: String) -> Data {
        Data(value.utf8)
    }

    // MARK: - The regression that reverted streaming the first time

    ///
    /// The framework may serve any batch from a fresh `Enumerator`, and therefore a fresh buffer.
    /// That instance must still see that a producer is running and keep the sequence open.
    ///
    func testASecondEnumeratorDoesNotEndASequenceWhoseProducerIsStillRunning() throws {
        let producing = makeBuffer()
        let token = try XCTUnwrap(
            producing.primeStreaming(key: "anchor-1", incomingAnchorRawValue: anchorData("incoming"))
        )
        producing.append(token: token, updated: [metadata("wave1")], deleted: [])

        // First batch through the producing enumerator.
        let first = producing.takeBatch(maxItems: 100)
        XCTAssertEqual(first.updated.map(\.ocId), ["wave1"])
        XCTAssertTrue(first.moreComing)

        // The framework now invalidates that enumerator and continues on a new one, which shares
        // only the database. The producer is still walking and has not appended its next wave yet.
        let continuation = makeBuffer()
        let continuationKey = try XCTUnwrap(
            String(data: XCTUnwrap(first.continuationAnchorRawValue), encoding: .utf8)
        )
        XCTAssertTrue(
            continuation.isPrimed(forKey: continuationKey),
            "The fresh enumerator must adopt the durable session."
        )

        let second = continuation.takeBatch(maxItems: 100)
        XCTAssertTrue(
            second.moreComing,
            "A fresh enumerator must not end a sequence whose producer is still running — doing so tells the framework we are synced and silently drops everything the scan has yet to find."
        )

        // And the session must survive for the producer to keep appending to.
        producing.append(token: token, updated: [metadata("wave2")], deleted: [])
        let third = continuation.takeBatch(maxItems: 100)
        XCTAssertEqual(
            third.updated.map(\.ocId), ["wave2"],
            "The still-running producer's later waves must still reach the framework."
        )
    }

    ///
    /// The other half: a producer that died must not wedge the sequence open forever. Once its
    /// liveness lapses, any enumerator may finish the sequence — on the incoming anchor, so the next
    /// signal re-derives whatever the dead scan never reached.
    ///
    func testAnAbandonedProducerLetsTheSequenceFinishOnTheIncomingAnchor() throws {
        let producing = makeBuffer()
        let token = try XCTUnwrap(
            producing.primeStreaming(key: "anchor-2", incomingAnchorRawValue: anchorData("incoming"))
        )
        producing.append(token: token, updated: [metadata("first"), metadata("second")], deleted: [])

        // One batch, so the next enumerator has a continuation anchor to adopt the session by.
        let first = producing.takeBatch(maxItems: 1)
        XCTAssertEqual(first.updated.map(\.ocId), ["first"])
        let continuationKey = try XCTUnwrap(
            String(data: XCTUnwrap(first.continuationAnchorRawValue), encoding: .utf8)
        )

        // The producing process goes away mid-scan: its liveness is never refreshed again.
        producing.expireProducerLivenessForTesting(token: token)

        let continuation = makeBuffer()
        XCTAssertTrue(continuation.isPrimed(forKey: continuationKey))

        var batch = continuation.takeBatch(maxItems: 100)
        XCTAssertEqual(batch.updated.map(\.ocId), ["second"], "Whatever was stored is still delivered.")

        while batch.moreComing {
            batch = continuation.takeBatch(maxItems: 100)
        }

        XCTAssertEqual(
            batch.finalAnchorRawValue, anchorData("incoming"),
            "An abandoned stream keeps its incoming anchor so the next signal re-derives."
        )
    }

    // MARK: - Core streaming contract

    /// While a producer is appending, the sequence must never be declared finished.
    func testSequenceStaysOpenWhileTheProducerIsStillAppending() throws {
        let buffer = makeBuffer()
        let token = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-3", incomingAnchorRawValue: anchorData("incoming"))
        )

        buffer.append(token: token, updated: [metadata("wave1")], deleted: [])
        XCTAssertTrue(buffer.takeBatch(maxItems: 100).moreComing)

        buffer.append(token: token, updated: [metadata("wave2")], deleted: [])
        XCTAssertTrue(buffer.takeBatch(maxItems: 100).moreComing)
    }

    /// Only once the producer reports done may the sequence end, on the anchor it supplied.
    func testFinishingTheProducerClosesTheSequenceOnTheFinalAnchor() throws {
        let buffer = makeBuffer()
        let token = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-4", incomingAnchorRawValue: anchorData("incoming"))
        )

        buffer.append(token: token, updated: [metadata("a")], deleted: [])
        buffer.finishStreaming(token: token, finalAnchorRawValue: anchorData("final"), incomplete: false)

        let batch = buffer.takeBatch(maxItems: 100)
        XCTAssertEqual(batch.updated.map(\.ocId), ["a"])
        XCTAssertFalse(batch.moreComing)
        XCTAssertEqual(batch.finalAnchorRawValue, anchorData("final"))
    }

    /// A scan that could not read part of the working set keeps the incoming anchor.
    func testAnIncompleteScanRetainsTheIncomingAnchor() throws {
        let buffer = makeBuffer()
        let token = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-5", incomingAnchorRawValue: anchorData("incoming"))
        )

        buffer.append(token: token, updated: [metadata("a")], deleted: [])
        buffer.finishStreaming(token: token, finalAnchorRawValue: anchorData("incoming"), incomplete: true)

        XCTAssertTrue(buffer.isPrimedIncomplete())
    }

    /// A fresh anchor mid-scan replaces the session; the superseded producer must not touch it.
    func testASupersededProducerCannotTouchTheNewSession() throws {
        let buffer = makeBuffer()
        let stale = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-6", incomingAnchorRawValue: anchorData("incoming"))
        )
        buffer.append(token: stale, updated: [metadata("stale")], deleted: [])

        buffer.reset()
        let current = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-7", incomingAnchorRawValue: anchorData("incoming-2"))
        )
        buffer.append(token: current, updated: [metadata("fresh")], deleted: [])

        buffer.append(token: stale, updated: [metadata("stale-late")], deleted: [])
        buffer.finishStreaming(token: stale, finalAnchorRawValue: anchorData("stale-final"), incomplete: false)

        let batch = buffer.takeBatch(maxItems: 100)
        XCTAssertEqual(batch.updated.map(\.ocId), ["fresh"], "Only the live session's items are delivered.")
        XCTAssertTrue(batch.moreComing, "A stale producer must not end the live sequence.")
    }

    /// The drain waits for the first wave rather than answering empty with `moreComing`, which the
    /// framework would answer by calling straight back — a spin.
    func testAwaitingDeliverableItemsReturnsOnceAWaveLands() async throws {
        let buffer = makeBuffer()
        let token = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-8", incomingAnchorRawValue: anchorData("incoming"))
        )

        let late = metadata("late")
        let appender = Task { @Sendable in
            try? await Task.sleep(nanoseconds: 150_000_000)
            buffer.append(token: token, updated: [late], deleted: [])
        }

        await buffer.awaitDeliverableItems()
        _ = await appender.result

        XCTAssertEqual(buffer.takeBatch(maxItems: 100).updated.map(\.ocId), ["late"])
    }

    /// And must not hang when the producer finishes having produced nothing.
    func testAwaitingDeliverableItemsReturnsWhenTheProducerFinishesEmpty() async throws {
        let buffer = makeBuffer()
        let token = try XCTUnwrap(
            buffer.primeStreaming(key: "anchor-9", incomingAnchorRawValue: anchorData("incoming"))
        )

        let finalAnchor = anchorData("final")
        let finisher = Task { @Sendable in
            try? await Task.sleep(nanoseconds: 100_000_000)
            buffer.finishStreaming(token: token, finalAnchorRawValue: finalAnchor, incomplete: false)
        }

        await buffer.awaitDeliverableItems()
        _ = await finisher.result

        XCTAssertFalse(buffer.takeBatch(maxItems: 100).moreComing)
    }
}
