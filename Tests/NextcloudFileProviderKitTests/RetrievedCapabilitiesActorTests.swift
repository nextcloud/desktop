//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import Testing
@testable import TestInterface

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
        let setCaps = await actor.data[account1]

        #expect(setCaps?.retrievedAt == specificDate)
        #expect(setCaps?.capabilities != nil)
    }

    @Test func setOngoingFetchTrueCausesSuspension() async throws {
        let actor = RetrievedCapabilitiesActor()
        var awaiterDidProceed = false

        // 1. Mark fetch as ongoing
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        // 2. Attempt to await in a separate task
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            awaiterDidProceed = true // This should only become true after resumption
        }

        // 3. Give the awaitingTask a moment to potentially run and suspend
        try await Task.sleep(for: .milliseconds(100))
        #expect(!awaiterDidProceed, "`awaitFetchCompletion` should suspend if fetch is ongoing.")

        // 4. Clean up: complete the fetch to allow the task to finish
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        await awaitingTask.value // Ensure the task fully completes
        #expect(awaiterDidProceed, "Awaiter should proceed after fetch is no longer ongoing.")
    }

    @Test func setOngoingFetchFalseResumesAwaiter() async throws {
        let actor = RetrievedCapabilitiesActor()
        var awaiterCompleted = false

        // 1. Mark fetch as ongoing and start an awaiter
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            awaiterCompleted = true
        }

        // 2. Ensure it's waiting
        try await Task.sleep(for: .milliseconds(100))
        #expect(awaiterCompleted == false, "Awaiter should be suspended initially.")

        // 3. Mark fetch as not ongoing, which should resume the awaiter
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        // 4. Await the task's completion and check the flag
        await awaitingTask.value
        #expect(awaiterCompleted, "Awaiter should complete after `setOngoingFetch(false)`.")
    }

    @Test func awaitFetchCompletionReturnsImmediately() async throws {
        let actor = RetrievedCapabilitiesActor()
        var didAwaiterCompleteImmediately = false

        let task = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            didAwaiterCompleteImmediately = true
        }
        await task.value

        #expect(
            didAwaiterCompleteImmediately,
            "`awaitFetchCompletion` should let the task complete quickly if no fetch is ongoing."
        )
    }

    @Test func awaitFetchCompletion_suspendsAndResumes_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        var didAwaiterComplete = false

        // 1. Mark fetch as ongoing
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        // 2. Start task that awaits
        let awaitingTask = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            didAwaiterComplete = true
        }

        // 3. Check for suspension (indirectly)
        try await Task.sleep(for: .milliseconds(100))
        #expect(!didAwaiterComplete, "Awaiter should be suspended while fetch is ongoing.")

        // 4. Mark fetch as completed
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        // 5. Awaiter should complete
        await awaitingTask.value
        #expect(didAwaiterComplete, "Awaiter should complete after fetch is no longer ongoing.")
    }

    @Test func awaitFetchCompletion_multipleAwaiters_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        var awaiter1Complete = false
        var awaiter2Complete = false

        await actor.setOngoingFetch(forAccount: account1, ongoing: true)

        let task1 = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            awaiter1Complete = true
        }
        let task2 = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            awaiter2Complete = true
        }

        try await Task.sleep(for: .milliseconds(100))
        #expect(!awaiter1Complete && !awaiter2Complete, "Both awaiters should be suspended.")

        await actor.setOngoingFetch(forAccount: account1, ongoing: false)

        await task1.value
        await task2.value
        #expect(
            awaiter1Complete && awaiter2Complete,
            "Both awaiters should complete after fetch is no longer ongoing."
        )
    }

    @Test func setOngoingFetch_false_isolatesAccountResumption_behavioral() async throws {
        let actor = RetrievedCapabilitiesActor()
        var acc1AwaiterDone = false
        var acc2AwaiterDone = false

        // Start fetches for both accounts
        await actor.setOngoingFetch(forAccount: account1, ongoing: true)
        await actor.setOngoingFetch(forAccount: account2, ongoing: true)

        // Setup awaiters
        let taskAcc1 = Task {
            await actor.awaitFetchCompletion(forAccount: account1)
            acc1AwaiterDone = true
        }
        let taskAcc2 = Task {
            await actor.awaitFetchCompletion(forAccount: account2)
            acc2AwaiterDone = true
        }

        try await Task.sleep(for: .milliseconds(100)) // Allow tasks to suspend
        #expect(!acc1AwaiterDone && !acc2AwaiterDone, "Both awaiters initially suspended.")

        // Complete fetch for account1 ONLY
        await actor.setOngoingFetch(forAccount: account1, ongoing: false)
        await taskAcc1.value

        #expect(acc1AwaiterDone, "Awaiter for account1 should complete.")
        #expect(!acc2AwaiterDone, "Awaiter for account2 should still be suspended.")

        // Complete fetch for account2
        await actor.setOngoingFetch(forAccount: account2, ongoing: false)
        await taskAcc2.value // Wait for acc2's awaiter to complete

        #expect(acc2AwaiterDone, "Awaiter for account2 should now complete.")
    }
}
