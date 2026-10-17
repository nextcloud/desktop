//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import NextcloudKit
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

    @Test func cancellingSharedFetchWaiterPreservesOtherWaitersAndCache() async throws {
        let actor = RetrievedCapabilitiesActor()
        let capabilitiesData = try #require(mockCapabilities.data(using: .utf8))
        let capabilities = try #require(Capabilities(data: capabilitiesData))
        let (waiting, waitingContinuation) = AsyncStream<Void>.makeStream()
        let (release, releaseContinuation) = AsyncStream<Void>.makeStream()
        defer {
            waitingContinuation.finish()
            releaseContinuation.finish()
        }
        let first = Task {
            await actor.currentCapabilities(forAccount: account1, onWaiting: { waitingContinuation.yield(()) }) { _ in
                do {
                    _ = try await nextTestValue(from: release)
                } catch {
                    Issue.record("Shared fetch was not released: \(error)")
                }
                #expect(!Task.isCancelled)
                return (account1, capabilities, nil, .success)
            }
        }
        defer { first.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        let second = Task {
            let result = await actor.currentCapabilities(forAccount: account1, onWaiting: { waitingContinuation.yield(()) }) { _ in
                Issue.record("A second waiter must reuse the shared fetch")
                return (account1, nil, nil, .invalidResponseError)
            }
            #expect(await actor.getCapabilities(for: account1)?.capabilities == capabilities)
            return result
        }
        defer { second.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        first.cancel()

        #expect(try await testTaskValue(of: first).error == .cancelled)
        #expect(await actor.ongoingFetches.contains(account1))
        releaseContinuation.yield(())
        let result = try await testTaskValue(of: second)
        #expect(result.error == .success)
        #expect(result.capabilities == capabilities)
        #expect(await actor.ongoingFetches.isEmpty)
        let cached = await actor.currentCapabilities(forAccount: account1) { _ in
            Issue.record("A completed fetch must populate the cache before releasing waiters")
            return (account1, nil, nil, .invalidResponseError)
        }
        #expect(cached.capabilities == capabilities)
    }

    @Test(arguments: [NKError.invalidResponseError, NKError(errorCode: 503, errorDescription: "Unavailable")])
    func sharedFetchFailureReachesAllWaitersAndAllowsRetry(error: NKError) async throws {
        let actor = RetrievedCapabilitiesActor()
        let (waiting, waitingContinuation) = AsyncStream<Void>.makeStream()
        let (release, releaseContinuation) = AsyncStream<Void>.makeStream()
        defer {
            waitingContinuation.finish()
            releaseContinuation.finish()
        }
        let first = Task {
            await actor.currentCapabilities(forAccount: account1, onWaiting: { waitingContinuation.yield(()) }) { _ in
                do {
                    _ = try await nextTestValue(from: release)
                } catch {
                    Issue.record("Shared fetch was not released: \(error)")
                }
                return (account1, nil, nil, error)
            }
        }
        defer { first.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        let second = Task {
            await actor.currentCapabilities(forAccount: account1, onWaiting: { waitingContinuation.yield(()) }) { _ in
                Issue.record("An overlapping caller must share the failed request")
                return (account1, nil, nil, .success)
            }
        }
        defer { second.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        releaseContinuation.yield(())
        #expect(try await testTaskValue(of: first).error == error)
        #expect(try await testTaskValue(of: second).error == error)
        #expect(await actor.getCapabilities(for: account1) == nil)
        #expect(await actor.ongoingFetches.isEmpty)

        let capabilitiesData = try #require(mockCapabilities.data(using: .utf8))
        let capabilities = try #require(Capabilities(data: capabilitiesData))
        let retry = await actor.currentCapabilities(forAccount: account1) { _ in (account1, capabilities, nil, .success) }
        #expect(retry.error == .success)
        #expect(retry.capabilities == capabilities)
    }

    @Test func cancelledLookupWaitingForDirectFetchDoesNotStartAnotherFetch() async throws {
        let actor = RetrievedCapabilitiesActor()
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let task = Task {
            await actor.currentCapabilities(forAccount: account1, onWaiting: { continuation.yield(()) }) { _ in
                Issue.record("A cancelled waiter must not start a fetch after the direct fetch ends")
                return (account1, nil, nil, .invalidResponseError)
            }
        }
        defer { task.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        task.cancel()
        #expect(try await testTaskValue(of: task).error == .cancelled)
        #expect(await actor.ongoingFetches.contains(account1))
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
    }

    @Test func resetReleasesSharedFetchWaiters() async throws {
        let actor = RetrievedCapabilitiesActor()
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: #require(URL(string: "https://example.invalid/capabilities")))
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        let (release, releaseContinuation) = AsyncStream<Void>.makeStream()
        defer {
            continuation.finish()
            releaseContinuation.finish()
        }
        let task = Task {
            await actor.currentCapabilities(forAccount: account1) { taskHandler in
                taskHandler(networkTask)
                continuation.yield(())
                _ = try? await nextTestValue(from: release)
                return (account1, nil, nil, .invalidResponseError)
            }
        }
        defer { task.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        let worker = try #require(await actor.sharedFetches[account1]?.task)
        await actor.reset()
        #expect(try await testTaskValue(of: task).error == .cancelled)
        #expect(await actor.ongoingFetches.isEmpty)
        #expect(worker.isCancelled)
        #expect(networkTask.state == .canceling || networkTask.state == .completed)
        #expect(await actor.getCapabilities(for: account1) == nil)
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
