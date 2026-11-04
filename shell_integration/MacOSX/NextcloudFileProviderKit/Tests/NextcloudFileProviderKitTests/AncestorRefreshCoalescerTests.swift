//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

/// Records what the coalescer nudged and how many nudges have finished.
private actor NudgeRecorder {
    /// In the order the drains issued them, including repeats.
    private(set) var nudged: [NSFileProviderItemIdentifier] = []

    private(set) var finished = 0

    func begin(_ identifier: NSFileProviderItemIdentifier) {
        nudged.append(identifier)
    }

    func end() {
        finished += 1
    }

    var count: Int {
        nudged.count
    }
}

private struct NudgeFailure: Error {}

struct AncestorRefreshCoalescerTests {
    private static let containerA = NSFileProviderItemIdentifier("containerA")
    private static let containerB = NSFileProviderItemIdentifier("containerB")
    private static let containerC = NSFileProviderItemIdentifier("containerC")

    private let log = FileProviderLogMock()

    /// Every file ocId is named `<container>/<file>`, so this stands in for the database walk.
    private static func containers(of ocIds: Set<String>) -> Set<NSFileProviderItemIdentifier> {
        Set(ocIds.map { NSFileProviderItemIdentifier(String($0.split(separator: "/")[0])) })
    }

    /// Poll until `condition` holds, failing the test if it has not held by `timeout`, which is only a backstop against a hang.
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

    private func recordingNudge(_ recorder: NudgeRecorder) -> @Sendable (NSFileProviderItemIdentifier) async throws -> Void {
        { identifier in
            await recorder.begin(identifier)
        }
    }

    @Test func eachDomainHasItsOwnCoalescer() {
        let first = NSFileProviderDomainIdentifier("AncestorRefreshCoalescerTests.first")
        let second = NSFileProviderDomainIdentifier("AncestorRefreshCoalescerTests.second")

        #expect(AncestorRefreshCoalescer.shared(for: first) === AncestorRefreshCoalescer.shared(for: first))
        #expect(AncestorRefreshCoalescer.shared(for: first) !== AncestorRefreshCoalescer.shared(for: second))
    }

    @Test func nudgesEachSharedContainerOnceForABurst() async throws {
        let recorder = NudgeRecorder()
        let window = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })
        let ocIds = Set((0 ..< 50).map { "containerA/file\($0)" }).union((0 ..< 50).map { "containerB/file\($0)" })

        for ocId in ocIds {
            coalescer.enqueue(ocIds: [ocId], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        }

        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")
        #expect(await recorder.count == 0, "A container was nudged before its window closed.")

        await window.release()
        try await waitUntil({ await recorder.count >= 2 }, "The drain never nudged both containers.")

        coalescer.enqueue(ocIds: ["containerC/file1"], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        try await waitUntil({ await window.entries == 2 }, "The sentinel never reached a window.")
        await window.release()
        try await waitUntil({ await recorder.count >= 3 }, "The sentinel was never nudged.")

        let nudged = await recorder.nudged
        #expect(nudged.count == 3)
        #expect(Set(nudged.prefix(2)) == [Self.containerA, Self.containerB])
        #expect(nudged.last == Self.containerC)
    }

    @Test func enqueuesDuringTheWindowJoinTheSameBatch() async throws {
        let recorder = NudgeRecorder()
        let window = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")

        coalescer.enqueue(ocIds: ["containerA/file2"], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        #expect(await recorder.count == 0, "A container was nudged before its window closed.")

        await window.release()
        try await waitUntil({ await recorder.count >= 1 }, "The batch was never nudged.")

        coalescer.enqueue(ocIds: ["containerC/file1"], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        try await waitUntil({ await window.entries == 2 }, "The sentinel never reached a window.")
        await window.release()
        try await waitUntil({ await recorder.count >= 2 }, "The sentinel was never nudged.")

        #expect(await recorder.nudged == [Self.containerA, Self.containerC])
    }

    @Test func aContainerIsNudgedAgainInALaterWindow() async throws {
        let recorder = NudgeRecorder()
        let window = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")
        await window.release()
        try await waitUntil({ await recorder.count >= 1 }, "The first batch was never nudged.")

        coalescer.enqueue(ocIds: ["containerA/file2"], ancestors: Self.containers, nudge: recordingNudge(recorder), log: log)
        try await waitUntil({ await window.entries == 2 }, "A later enqueue never reached a window.")
        await window.release()
        try await waitUntil({ await recorder.count >= 2 }, "A later enqueue was dropped.")

        #expect(await recorder.nudged == [Self.containerA, Self.containerA])
    }

    @Test func drainsDoNotOverlap() async throws {
        let recorder = NudgeRecorder()
        let window = Gate()
        let nudgeInFlight = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })
        let parkedNudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void = { identifier in
            await recorder.begin(identifier)
            await nudgeInFlight.wait()
        }

        coalescer.enqueue(ocIds: ["containerA/file1", "containerC/file1"], ancestors: Self.containers, nudge: parkedNudge, log: log)
        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")
        await window.release()
        try await waitUntil({ await nudgeInFlight.entries == 1 }, "The first nudge never started.")

        coalescer.enqueue(ocIds: ["containerB/file1"], ancestors: Self.containers, nudge: parkedNudge, log: log)
        await nudgeInFlight.release()
        try await waitUntil({ await nudgeInFlight.entries == 2 }, "The rest of the first batch was never nudged.")
        #expect(await window.entries == 1, "An enqueue during the nudges started a second drain.")

        await nudgeInFlight.release()
        try await waitUntil({ await window.entries == 2 }, "The second batch never reached a window.")
        await window.release()
        try await waitUntil({ await nudgeInFlight.entries == 3 }, "The second batch was never nudged.")
        await nudgeInFlight.release()

        let nudged = await recorder.nudged
        #expect(Set(nudged.prefix(2)) == [Self.containerA, Self.containerC])
        #expect(nudged.last == Self.containerB)
    }

    @Test func aFailedNudgeDoesNotStopTheRestOfTheBatch() async throws {
        let recorder = NudgeRecorder()
        let window = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })
        let failingNudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void = { identifier in
            await recorder.begin(identifier)

            throw NudgeFailure()
        }

        coalescer.enqueue(ocIds: ["containerA/file1", "containerB/file1"], ancestors: Self.containers, nudge: failingNudge, log: log)
        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")
        await window.release()
        try await waitUntil({ await recorder.count >= 2 }, "A failed nudge stopped the rest of the batch.")

        coalescer.enqueue(ocIds: ["containerA/file2"], ancestors: Self.containers, nudge: failingNudge, log: log)
        try await waitUntil({ await window.entries == 2 }, "A failed nudge left the coalescer unable to drain again.")
        await window.release()
        try await waitUntil({ await recorder.count >= 3 }, "The container whose nudge failed was not nudged by a later enqueue.")

        let nudged = await recorder.nudged
        #expect(Set(nudged.prefix(2)) == [Self.containerA, Self.containerB])
        #expect(nudged.last == Self.containerA)
    }

    @Test func cancelLeavesPendingWorkToTheNextDrain() async throws {
        let cancelled = NudgeRecorder()
        let current = NudgeRecorder()
        let window = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, nudge: recordingNudge(cancelled), log: log)
        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")
        coalescer.cancel()
        await window.release()

        coalescer.enqueue(ocIds: ["containerC/file1"], ancestors: Self.containers, nudge: recordingNudge(current), log: log)
        try await waitUntil({ await window.entries == 2 }, "An enqueue after cancel() never started a drain.")
        await window.release()
        try await waitUntil({ await current.count >= 2 }, "The next drain did not nudge the pending batch along with its own.")

        #expect(await cancelled.nudged.isEmpty, "A cancelled drain nudged through the closures it was given.")
        #expect(await Set(current.nudged) == [Self.containerA, Self.containerC])
    }

    @Test func cancelStopsADrainThatIsNudging() async throws {
        let cancelled = NudgeRecorder()
        let current = NudgeRecorder()
        let window = Gate()
        let nudgeInFlight = Gate()
        let coalescer = AncestorRefreshCoalescer(waitForWindow: { await window.wait() })
        let parkedNudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void = { identifier in
            await cancelled.begin(identifier)
            await nudgeInFlight.wait()
            await cancelled.end()
        }

        coalescer.enqueue(ocIds: ["containerA/file1", "containerB/file1"], ancestors: Self.containers, nudge: parkedNudge, log: log)
        try await waitUntil({ await window.entries == 1 }, "The drain never waited for its window.")
        await window.release()
        try await waitUntil({ await nudgeInFlight.entries == 1 }, "The first nudge never started.")

        coalescer.cancel()
        await nudgeInFlight.release()
        try await waitUntil({ await cancelled.finished == 1 }, "The nudge in flight never finished.")
        coalescer.enqueue(ocIds: ["containerC/file1"], ancestors: Self.containers, nudge: recordingNudge(current), log: log)

        try await waitUntil({ await window.entries == 2 }, "An enqueue after cancel() never started a drain.")
        // The cancelled drain hands its batch back just after its last nudge, which can land after the new drain's claim or after it has ended, so every poll starts a drain if none runs and opens its window until all three arrive.
        let currentNudge = recordingNudge(current)
        let log = log
        try await waitUntil({
            if await current.count >= 3 {
                return true
            }
            coalescer.enqueue(ocIds: [], ancestors: Self.containers, nudge: currentNudge, log: log)
            await window.release()
            return false
        }, "The drain started after cancel() did not take over the cancelled drain's batch.")

        #expect(await cancelled.count == 1, "The cancelled drain kept nudging after the nudge it had in flight.")
        #expect(await Set(current.nudged) == [Self.containerA, Self.containerB, Self.containerC], "The batch the cancelled drain had claimed must be nudged by the next drain.")
    }

    /// Invalidating an extension must stop its own domain's drain, so the pending batch waits for the next enqueue's drain.
    @Test func invalidatingTheExtensionStopsItsDomainsDrain() async throws {
        let cancelled = NudgeRecorder()
        let current = NudgeRecorder()
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier("AncestorRefreshCoalescerTests-\(UUID().uuidString)"), displayName: "Test")
        let coalescer = AncestorRefreshCoalescer.shared(for: domain.identifier)
        let ext = FileProviderExtension(domain: domain)

        coalescer.enqueue(ocIds: ["containerA/file1"], ancestors: Self.containers, nudge: recordingNudge(cancelled), log: log)
        ext.invalidate()
        coalescer.enqueue(ocIds: ["containerB/file1"], ancestors: Self.containers, nudge: recordingNudge(current), log: log)
        try await waitUntil({ await current.count >= 2 }, "The drain started after invalidate() did not nudge the pending batch.")

        #expect(await cancelled.nudged.isEmpty, "The invalidated instance's drain kept nudging.")
        #expect(await Set(current.nudged) == [Self.containerA, Self.containerB])
    }
}
