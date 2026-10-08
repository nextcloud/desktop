//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import Testing
@testable import TestInterface

@Suite(.timeLimit(.minutes(1)))
struct RetrievedCapabilitiesActorTests {
    let account1 = "acc1"
    let account2 = "acc2"

    private func awaitWaiter(_ task: Task<Void, Never>, actor: RetrievedCapabilitiesActor) async throws {
        do {
            try await testTaskValue(of: task)
        } catch {
            // Release any waiter whose cancellation handler failed before reporting the timeout.
            await actor.setOngoingFetch(forAccount: account1, ongoing: false)
            await actor.setOngoingFetch(forAccount: account2, ongoing: false)
            throw error
        }
    }

    private func awaitRegistration(in stream: AsyncStream<Void>, actor: RetrievedCapabilitiesActor) async throws {
        do {
            try #require(try await nextTestValue(from: stream) != nil)
        } catch {
            await actor.setOngoingFetch(forAccount: account1, ongoing: false)
            await actor.setOngoingFetch(forAccount: account2, ongoing: false)
            throw error
        }
    }

    @Test func alreadyCancelledWaiterDoesNotEnqueue() async throws {
        let actor = RetrievedCapabilitiesActor()
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: {
                Issue.record("Already cancelled waiter must not enqueue")
            })
        }
        defer { task.cancel() }

        try await awaitWaiter(task, actor: actor)
        #expect(await actor.ongoingFetches.contains(account1))
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
    }

    @Test(arguments: [false, true])
    func cancellationAndFetchCompletionResumeTheWaiterOnce(completeFirst: Bool) async throws {
        let actor = RetrievedCapabilitiesActor()
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let task = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
        }
        defer { task.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        if completeFirst {
            await actor.setOngoingFetch(forAccount: account1, ongoing: false)
            task.cancel()
        } else {
            task.cancel()
            await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        }
        try await awaitWaiter(task, actor: actor)
        #expect(await actor.ongoingFetches.isEmpty)
    }

    @Test func cancellingOneWaiterLeavesTheFetchAndOtherWaitersRunning() async throws {
        let actor = RetrievedCapabilitiesActor()
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let otherWaiterFinished = Expectation("Other waiter finishes")
        let first = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
        }
        let second = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await otherWaiterFinished.fulfill()
        }
        defer {
            first.cancel()
            second.cancel()
        }
        try await awaitRegistration(in: waiting, actor: actor)
        try await awaitRegistration(in: waiting, actor: actor)
        first.cancel()

        try await awaitWaiter(first, actor: actor)
        #expect(await actor.ongoingFetches.contains(account1))
        #expect(await otherWaiterFinished.isFulfilled == false)

        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        try await awaitWaiter(second, actor: actor)
        #expect(await otherWaiterFinished.isFulfilled)
    }

    @Test func setCapabilitiesCompletes() async throws {
        let actor = RetrievedCapabilitiesActor() // New instance for the test
        let capsData = try #require(mockCapabilities.data(using: .utf8))
        let caps = try #require(Capabilities(data: capsData))
        let specificDate = Date(timeIntervalSince1970: 1_234_567_890)

        // We call the public API.
        await actor.setCapabilities(forAccount: account1, capabilities: caps, retrievedAt: specificDate)
        let setCaps = await actor.getCapabilities(for: account1)

        #expect(setCaps?.retrievedAt == specificDate)
        #expect(setCaps?.capabilities != nil)
    }

    @Test func setOngoingFetchTrueCausesSuspension() async throws {
        let actor = RetrievedCapabilitiesActor()
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let awaiterDidProceed = Expectation("awaiterDidProceed")

        // 1. Mark fetch as ongoing
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        // 2. Attempt to await in a separate task
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await awaiterDidProceed.fulfill()
        }

        // 3. Wait until the awaitingTask is registered.
        try await awaitRegistration(in: waiting, actor: actor)
        #expect(await awaiterDidProceed.isFulfilled == false, "`awaitFetchCompletion` should suspend if fetch is ongoing.")

        // 4. Clean up: complete the fetch to allow the task to finish
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        try await awaitWaiter(awaitingTask, actor: actor) // Ensure the task fully completes
        #expect(await awaiterDidProceed.isFulfilled, "Awaiter should proceed after fetch is no longer ongoing.")
    }

    @Test func setOngoingFetchFalseResumesAwaiter() async throws {
        let actor = RetrievedCapabilitiesActor()
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let awaiterCompleted = Expectation("awaiterCompleted")

        // 1. Mark fetch as ongoing and start an awaiter
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await awaiterCompleted.fulfill()
        }

        // 2. Ensure it's waiting
        try await awaitRegistration(in: waiting, actor: actor)
        #expect(await awaiterCompleted.isFulfilled == false, "Awaiter should be suspended initially.")

        // 3. Mark fetch as not ongoing, which should resume the awaiter
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        // 4. Await the task's completion and check the flag
        try await awaitWaiter(awaitingTask, actor: actor)
        #expect(await awaiterCompleted.isFulfilled, "Awaiter should complete after `setOngoingFetch(false)`.")
    }

    @Test func awaitFetchCompletionReturnsImmediately() async {
        let actor = RetrievedCapabilitiesActor()

        await confirmation("did awaiter complete immediately") { didAwaiterCompleteImmediately in
            await actor.awaitFetchCompletion(forAccount: account1)
            didAwaiterCompleteImmediately()
        }
    }

    @Test func awaitFetchCompletion_suspendsAndResumes_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let didAwaiterComplete = Expectation("didAwaiterComplete")

        // 1. Mark fetch as ongoing
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        // 2. Start task that awaits
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await didAwaiterComplete.fulfill()
        }

        // 3. Wait until the awaitingTask is registered.
        try await awaitRegistration(in: waiting, actor: actor)
        #expect(await didAwaiterComplete.isFulfilled == false, "Awaiter should be suspended while fetch is ongoing.")

        // 4. Mark fetch as completed
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        // 5. Awaiter should complete
        try await awaitWaiter(awaitingTask, actor: actor)
        #expect(await didAwaiterComplete.isFulfilled, "Awaiter should complete after fetch is no longer ongoing.")
    }

    @Test func awaitFetchCompletion_multipleAwaiters_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let awaiter1Complete = Expectation("awaiter1Complete")
        let awaiter2Complete = Expectation("awaiter2Complete")

        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        let task1 = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await awaiter1Complete.fulfill()
        }
        let task2 = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await awaiter2Complete.fulfill()
        }

        try await awaitRegistration(in: waiting, actor: actor)
        try await awaitRegistration(in: waiting, actor: actor)

        var firstFulfillment = await awaiter1Complete.isFulfilled
        var secondFulfillment = await awaiter2Complete.isFulfilled
        #expect(firstFulfillment == false && secondFulfillment == false, "Both awaiters should be suspended.")

        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        try await awaitWaiter(task1, actor: actor)
        try await awaitWaiter(task2, actor: actor)

        firstFulfillment = await awaiter1Complete.isFulfilled
        secondFulfillment = await awaiter2Complete.isFulfilled
        #expect(firstFulfillment && secondFulfillment, "Both awaiters should complete after fetch is no longer ongoing.")
    }

    @Test func setOngoingFetch_false_isolatesAccountResumption_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let acc1AwaiterDone = Expectation("acc1AwaiterDone")
        let acc2AwaiterDone = Expectation("acc2AwaiterDone")

        // Start fetches for both accounts
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        await actor.setOngoingFetch(forAccount: account2, ongoing: true)

        // Setup awaiters
        let taskAcc1 = Task {
            await actor.awaitFetchCompletion(forAccount: account1, onWaiting: { continuation.yield(()) })
            await acc1AwaiterDone.fulfill()
        }
        let taskAcc2 = Task {
            await actor.awaitFetchCompletion(forAccount: account2, onWaiting: { continuation.yield(()) })
            await acc2AwaiterDone.fulfill()
        }

        try await awaitRegistration(in: waiting, actor: actor)
        try await awaitRegistration(in: waiting, actor: actor)

        let firstFulfillment = await acc1AwaiterDone.isFulfilled
        let secondFulfillment = await acc2AwaiterDone.isFulfilled
        #expect(firstFulfillment == false && secondFulfillment == false, "Both awaiters initially suspended.")

        // Complete fetch for account1 ONLY
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        try await awaitWaiter(taskAcc1, actor: actor)

        #expect(await acc1AwaiterDone.isFulfilled, "Awaiter for account1 should complete.")
        #expect(await acc2AwaiterDone.isFulfilled == false, "Awaiter for account2 should still be suspended.")

        // Complete fetch for account2
        await actor.setOngoingFetch(forAccount: account2, ongoing: false)
        try await awaitWaiter(taskAcc2, actor: actor) // Wait for acc2's awaiter to complete

        #expect(await acc2AwaiterDone.isFulfilled, "Awaiter for account2 should now complete.")
    }
}
