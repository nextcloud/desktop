//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import Testing
@testable import TestInterface
import XCTest

@Suite("RetrievedCapabilitiesActor tests")
struct RetrievedCapabilitiesActorTests {
    let account1 = "acc1"
    let account2 = "acc2"

    @Test func setCapabilitiesCompletes() async {
        let actor = RetrievedCapabilitiesActor() // New instance for the test
        let capsData = mockCapabilities.data(using: .utf8)!
        let caps = Capabilities(data: capsData)!
        let specificDate = Date(timeIntervalSince1970: 1_234_567_890)

        // We call the public API.
        await actor.setCapabilities(forAccount: account1, capabilities: caps, retrievedAt: specificDate)
        let setCaps = await actor.getCapabilities(for: account1)

        #expect(setCaps?.retrievedAt == specificDate)
        #expect(setCaps?.capabilities != nil)
    }

    @Test func setOngoingFetchTrueCausesSuspension() async throws {
        let actor = RetrievedCapabilitiesActor()
        let awaiterDidProceed = Expectation("awaiterDidProceed")

        // 1. Mark fetch as ongoing
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        // 2. Attempt to await in a separate task
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            await awaiterDidProceed.fulfill()
        }

        // 3. Give the awaitingTask a moment to potentially run and suspend
        try await Task.sleep(for: .milliseconds(100))
        #expect(await awaiterDidProceed.isFulfilled == false, "`awaitFetchCompletion` should suspend if fetch is ongoing.")

        // 4. Clean up: complete the fetch to allow the task to finish
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        await awaitingTask.value // Ensure the task fully completes
        #expect(await awaiterDidProceed.isFulfilled, "Awaiter should proceed after fetch is no longer ongoing.")
    }

    @Test func setOngoingFetchFalseResumesAwaiter() async throws {
        let actor = RetrievedCapabilitiesActor()
        let awaiterCompleted = Expectation("awaiterCompleted")

        // 1. Mark fetch as ongoing and start an awaiter
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            await awaiterCompleted.fulfill()
        }

        // 2. Ensure it's waiting
        try await Task.sleep(for: .milliseconds(100))
        #expect(await awaiterCompleted.isFulfilled == false, "Awaiter should be suspended initially.")

        // 3. Mark fetch as not ongoing, which should resume the awaiter
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        // 4. Await the task's completion and check the flag
        await awaitingTask.value
        #expect(await awaiterCompleted.isFulfilled, "Awaiter should complete after `setOngoingFetch(false)`.")
    }

    @Test func awaitFetchCompletionReturnsImmediately() async throws {
        let actor = RetrievedCapabilitiesActor()

        await confirmation("did awaiter complete immediately") { didAwaiterCompleteImmediately in
            await actor.awaitFetchCompletion(forAccount: account1)
            didAwaiterCompleteImmediately()
        }
    }

    @Test func awaitFetchCompletion_suspendsAndResumes_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        let didAwaiterComplete = Expectation("didAwaiterComplete")

        // 1. Mark fetch as ongoing
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        // 2. Start task that awaits
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            await didAwaiterComplete.fulfill()
        }

        // 3. Check for suspension (indirectly)
        try await Task.sleep(for: .milliseconds(100))
        #expect(await didAwaiterComplete.isFulfilled == false, "Awaiter should be suspended while fetch is ongoing.")

        // 4. Mark fetch as completed
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        // 5. Awaiter should complete
        await awaitingTask.value
        #expect(await didAwaiterComplete.isFulfilled, "Awaiter should complete after fetch is no longer ongoing.")
    }

    @Test func awaitFetchCompletion_multipleAwaiters_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        let awaiter1Complete = Expectation("awaiter1Complete")
        let awaiter2Complete = Expectation("awaiter2Complete")

        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        let task1 = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            await awaiter1Complete.fulfill()
        }
        let task2 = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            await awaiter2Complete.fulfill()
        }

        try await Task.sleep(for: .milliseconds(100))

        var firstFulfillment = await awaiter1Complete.isFulfilled
        var secondFulfillment = await awaiter2Complete.isFulfilled
        #expect(await firstFulfillment == false && secondFulfillment == false, "Both awaiters should be suspended.")

        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        await task1.value
        await task2.value

        firstFulfillment = await awaiter1Complete.isFulfilled
        secondFulfillment = await awaiter2Complete.isFulfilled
        #expect(firstFulfillment && secondFulfillment, "Both awaiters should complete after fetch is no longer ongoing.")
    }

    @Test func setOngoingFetch_false_isolatesAccountResumption_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        let acc1AwaiterDone = Expectation("acc1AwaiterDone")
        let acc2AwaiterDone = Expectation("acc2AwaiterDone")

        // Start fetches for both accounts
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        await actor.setOngoingFetch(forAccount: account2, ongoing: true)

        // Setup awaiters
        let taskAcc1 = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            await acc1AwaiterDone.fulfill()
        }
        let taskAcc2 = Task {
            await actor.awaitFetchCompletion(forAccount: account2)
            await acc2AwaiterDone.fulfill()
        }

        try await Task.sleep(for: .milliseconds(100)) // Allow tasks to suspend

        let firstFulfillment = await acc1AwaiterDone.isFulfilled
        let secondFulfillment = await acc2AwaiterDone.isFulfilled
        #expect(await firstFulfillment == false && secondFulfillment == false, "Both awaiters initially suspended.")

        // Complete fetch for account1 ONLY
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        await taskAcc1.value

        #expect(await acc1AwaiterDone.isFulfilled, "Awaiter for account1 should complete.")
        #expect(await acc2AwaiterDone.isFulfilled == false, "Awaiter for account2 should still be suspended.")

        // Complete fetch for account2
        await actor.setOngoingFetch(forAccount: account2, ongoing: false)
        await taskAcc2.value // Wait for acc2's awaiter to complete

        #expect(await acc2AwaiterDone.isFulfilled, "Awaiter for account2 should now complete.")
    }
}
