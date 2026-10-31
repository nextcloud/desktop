//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

/// Records what the coalescer nudged and how many nudges were ever in flight at once.
private actor NudgeRecorder {
    /// In the order the drains issued them, including repeats.
    private(set) var nudged: [NSFileProviderItemIdentifier] = []

    private(set) var peakConcurrency = 0

    private var inFlight = 0

    func begin(_ identifier: NSFileProviderItemIdentifier) {
        nudged.append(identifier)
        inFlight += 1
        peakConcurrency = max(peakConcurrency, inFlight)
    }

    func end() {
        inFlight -= 1
    }

    var count: Int {
        nudged.count
    }
}

/// A value the synchronous `evictableState` closure can read and the test can change between windows.
private final class StateBox: @unchecked Sendable {
    private let lock = NSLock()
    private var state: Bool?

    init(_ state: Bool?) {
        self.state = state
    }

    var value: Bool? {
        get {
            lock.lock()
            defer { lock.unlock() }
            return state
        }
        set {
            lock.lock()
            defer { lock.unlock() }
            state = newValue
        }
    }
}

struct AncestorRefreshCoalescerTests {
    private static let window: UInt64 = 100_000_000
    private static let containerA = NSFileProviderItemIdentifier("containerA")
    private static let containerB = NSFileProviderItemIdentifier("containerB")

    private let logger = FileProviderLogger(category: "AncestorRefreshCoalescerTests", log: FileProviderLogMock())

    /// Every file ocId is named `<container>/<file>`, so this stands in for the database walk.
    private static func containers(of ocIds: Set<String>) -> Set<NSFileProviderItemIdentifier> {
        Set(ocIds.map { NSFileProviderItemIdentifier(String($0.split(separator: "/")[0])) })
    }

    ///
    /// Poll until `condition` holds, failing the test if it has not held by `timeout`.
    ///
    /// The timeout is only a backstop against a hang; every assertion is on what was recorded, never
    /// on how long it took, so a slow machine cannot turn a pass into a failure.
    ///
    private func waitUntil(_ condition: @Sendable () async -> Bool, timeout: Duration = .seconds(10), _ comment: Comment, sourceLocation: SourceLocation = #_sourceLocation) async throws {
        let deadline = ContinuousClock().now + timeout

        while ContinuousClock().now < deadline {
            if await condition() {
                return
            }
            try await Task.sleep(for: .milliseconds(5))
        }

        Issue.record(comment, sourceLocation: sourceLocation)
    }

    @Test func nudgesEachSharedContainerOnceForABurst() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)
        let ocIds = Set((0 ..< 50).map { "containerA/file\($0)" }).union((0 ..< 50).map { "containerB/file\($0)" })

        for ocId in ocIds {
            coalescer.enqueue(ocIds: [ocId], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        }

        try await waitUntil({ await recorder.count >= 2 }, "The drain never nudged both containers.")
        try await Task.sleep(for: .milliseconds(400))

        #expect(await recorder.nudged.count == 2)
        #expect(await Set(recorder.nudged) == [Self.containerA, Self.containerB])
    }

    @Test func enqueuesDuringTheWindowJoinTheSameBatch() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        coalescer.enqueue(ocIds: ["containerB/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)

        try await waitUntil({ await recorder.count >= 2 }, "The second enqueue never reached a drain.")

        #expect(await Set(recorder.nudged) == [Self.containerA, Self.containerB])
    }

    @Test func enqueuesAfterTheBatchIsClaimedAreNotDropped() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        try await waitUntil({ await recorder.count >= 1 }, "The first batch never drained.")

        coalescer.enqueue(ocIds: ["containerB/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        try await waitUntil({ await recorder.count >= 2 }, "An enqueue made after the batch was claimed was dropped.")

        #expect(await Set(recorder.nudged) == [Self.containerA, Self.containerB])
    }

    /// A drain outlasting its window must not run beside the next one, or the rate cap is defeated.
    @Test func drainsDoNotOverlap() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)
        let slowNudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void = { identifier in
            await recorder.begin(identifier)
            try? await Task.sleep(for: .milliseconds(400))
            await recorder.end()
        }

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: slowNudge, logger: logger)
        try await waitUntil({ await recorder.count >= 1 }, "The first drain never started.")

        coalescer.enqueue(ocIds: ["containerB/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: slowNudge, logger: logger)
        try await waitUntil({ await recorder.count >= 2 }, "The second batch was never nudged.")

        #expect(await recorder.peakConcurrency == 1, "A drain started while the previous one was still nudging.")
    }

    /// The answer is one boolean folded into `metadataVersion`, so a stream that leaves it where it
    /// already was must not keep nudging.
    @Test func aContainerWhoseAnswerDidNotChangeIsNotNudgedAgain() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)
        let deadline = ContinuousClock().now + .milliseconds(600)
        var enqueued = 0

        while ContinuousClock().now < deadline {
            coalescer.enqueue(ocIds: ["containerA/file\(enqueued)"], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
            enqueued += 1
            try await Task.sleep(for: .milliseconds(10))
        }

        try await waitUntil({ await recorder.count >= 1 }, "The first download never nudged the container.")
        try await Task.sleep(for: .milliseconds(400))

        #expect(await recorder.nudged == [Self.containerA], "\(enqueued) downloads into one container whose answer flipped once must nudge it once.")
    }

    @Test func aContainerIsNudgedAgainWhenItsAnswerFlips() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)
        let state = StateBox(true)
        let enqueue = { (ocId: String) in
            coalescer.enqueue(ocIds: [ocId], ancestors: Self.containers, evictableState: { _ in state.value }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        }

        enqueue("containerA/file1")
        try await waitUntil({ await recorder.count >= 1 }, "The first materialisation never nudged the container.")

        state.value = false
        enqueue("containerA/file1")
        try await waitUntil({ await recorder.count >= 2 }, "The eviction that emptied the container never nudged it.")

        state.value = true
        enqueue("containerA/file2")
        try await waitUntil({ await recorder.count >= 3 }, "The re-materialisation never nudged the container.")

        #expect(await recorder.nudged == [Self.containerA, Self.containerA, Self.containerA])
    }

    @Test func aContainerWithAnUnknownAnswerIsAlwaysNudged() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, evictableState: { _ in nil }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        try await waitUntil({ await recorder.count >= 1 }, "A container with an unknown answer was not nudged.")

        coalescer.enqueue(ocIds: ["containerA/file2"], ancestors: Self.containers, evictableState: { _ in nil }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        try await waitUntil({ await recorder.count >= 2 }, "A second unknown answer was skipped as if it had been recorded.")
    }

    @Test func aFailedNudgeIsRetriedOnTheNextWindow() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: Self.window)
        let failFirst = StateBox(true)
        let nudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void = { identifier in
            await recorder.begin(identifier)
            await recorder.end()

            if failFirst.value == true {
                failFirst.value = false
                throw NSFileProviderError(.noSuchItem)
            }
        }

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: nudge, logger: logger)
        try await waitUntil({ await recorder.count >= 1 }, "The container was never nudged.")

        coalescer.enqueue(ocIds: ["containerA/file2"], ancestors: Self.containers, evictableState: { _ in true }, nudge: nudge, logger: logger)
        try await waitUntil({ await recorder.count >= 2 }, "A container whose nudge threw was treated as nudged and never retried.")
    }

    @Test func cancelDropsPendingWork() async throws {
        let recorder = NudgeRecorder()
        let coalescer = AncestorRefreshCoalescer(windowNanoseconds: 300_000_000)

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, evictableState: { _ in true }, nudge: { await recorder.begin($0); await recorder.end() }, logger: logger)
        coalescer.cancel()

        try await Task.sleep(for: .milliseconds(600))

        #expect(await recorder.nudged.isEmpty, "A cancelled coalescer nudged the batch it was told to drop.")
    }
}
