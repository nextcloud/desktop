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

    private func awaitRegistration(in stream: AsyncStream<Void>, actor: RetrievedCapabilitiesActor) async throws {
        do {
            try #require(try await nextTestValue(from: stream) != nil)
        } catch {
            await actor.reset()
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
        #expect(await actor.sharedFetches[account1] != nil)
        releaseContinuation.yield(())
        let result = try await testTaskValue(of: second)
        #expect(result.error == .success)
        #expect(result.capabilities == capabilities)
        #expect(await actor.sharedFetches.isEmpty)
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
        #expect(await actor.sharedFetches.isEmpty)

        let capabilitiesData = try #require(mockCapabilities.data(using: .utf8))
        let capabilities = try #require(Capabilities(data: capabilitiesData))
        let retry = await actor.currentCapabilities(forAccount: account1) { _ in (account1, capabilities, nil, .success) }
        #expect(retry.error == .success)
        #expect(retry.capabilities == capabilities)
    }

    @Test func resetReleasesSharedFetchWaiters() async throws {
        let actor = RetrievedCapabilitiesActor()
        let capabilitiesData = try #require(mockCapabilities.data(using: .utf8))
        let capabilities = try #require(Capabilities(data: capabilitiesData))
        await actor.setCapabilities(forAccount: account2, capabilities: capabilities)
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let url = try #require(URL(string: "https://example.invalid/capabilities"))
        let (networkTask, completion) = observedNetworkTask(in: session, url: url)
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
                return (account1, capabilities, nil, .success)
            }
        }
        defer { task.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        let worker = try #require(await actor.sharedFetches[account1]?.task)
        let second = Task {
            await actor.currentCapabilities(forAccount: account1, onWaiting: { continuation.yield(()) }) { _ in
                Issue.record("Reset must release the waiters of one shared request")
                return (account1, nil, nil, .invalidResponseError)
            }
        }
        defer { second.cancel() }
        try await awaitRegistration(in: waiting, actor: actor)
        await actor.reset()
        #expect(try await testTaskValue(of: task).error == .cancelled)
        #expect(try await testTaskValue(of: second).error == .cancelled)
        #expect(await actor.sharedFetches.isEmpty)
        #expect(worker.isCancelled)
        #expect(try await nextTestValue(from: completion) == NSURLErrorCancelled)
        try await testTaskValue(of: worker)
        #expect(await actor.getCapabilities(for: account1) == nil)
        #expect(await actor.getCapabilities(for: account2) == nil)
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
        #expect(setCaps?.capabilities == caps)
    }

    @Test func alreadyCancelledLookupDoesNotStartFetch() async throws {
        let actor = RetrievedCapabilitiesActor()
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            return await actor.currentCapabilities(forAccount: account1, onWaiting: {
                Issue.record("An already cancelled caller must not register a waiter")
            }) { _ in
                Issue.record("An already cancelled caller must not start a fetch")
                return (account1, nil, nil, .invalidResponseError)
            }
        }
        defer { task.cancel() }

        #expect(try await testTaskValue(of: task).error == .cancelled)
        #expect(await actor.sharedFetches.isEmpty)
    }

    @Test(arguments: [false, true])
    func cancellationAndCompletionReleaseSharedWaiterOnce(completeFirst: Bool) async throws {
        let actor = RetrievedCapabilitiesActor()
        let capabilitiesData = try #require(mockCapabilities.data(using: .utf8))
        let capabilities = try #require(Capabilities(data: capabilitiesData))
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (release, releaseContinuation) = AsyncStream<Void>.makeStream()
        defer {
            startedContinuation.finish()
            releaseContinuation.finish()
        }
        let task = Task {
            await actor.currentCapabilities(forAccount: account1) { _ in
                startedContinuation.yield(())
                _ = try? await nextTestValue(from: release)
                return (account1, capabilities, nil, .success)
            }
        }
        defer { task.cancel() }
        try await awaitRegistration(in: started, actor: actor)
        let worker = try #require(await actor.sharedFetches[account1]?.task)

        if completeFirst {
            releaseContinuation.yield(())
            #expect(try await testTaskValue(of: task).error == .success)
            task.cancel()
        } else {
            task.cancel()
            #expect(try await testTaskValue(of: task).error == .cancelled)
            releaseContinuation.yield(())
        }
        try await testTaskValue(of: worker)
        #expect(await actor.sharedFetches.isEmpty)
        #expect(await actor.getCapabilities(for: account1)?.capabilities == (completeFirst ? capabilities : nil))
    }

    @Test func completingOneAccountLeavesAnotherAccountFetching() async throws {
        let actor = RetrievedCapabilitiesActor()
        let capabilitiesData = try #require(mockCapabilities.data(using: .utf8))
        let capabilities = try #require(Capabilities(data: capabilitiesData))
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (releaseFirst, releaseFirstContinuation) = AsyncStream<Void>.makeStream()
        let (releaseSecond, releaseSecondContinuation) = AsyncStream<Void>.makeStream()
        defer {
            startedContinuation.finish()
            releaseFirstContinuation.finish()
            releaseSecondContinuation.finish()
        }
        let first = Task {
            await actor.currentCapabilities(forAccount: account1) { _ in
                startedContinuation.yield(())
                _ = try? await nextTestValue(from: releaseFirst)
                return (account1, capabilities, nil, .success)
            }
        }
        let second = Task {
            await actor.currentCapabilities(forAccount: account2) { _ in
                startedContinuation.yield(())
                _ = try? await nextTestValue(from: releaseSecond)
                return (account2, capabilities, nil, .success)
            }
        }
        defer {
            first.cancel()
            second.cancel()
        }
        try await awaitRegistration(in: started, actor: actor)
        try await awaitRegistration(in: started, actor: actor)

        releaseFirstContinuation.yield(())
        #expect(try await testTaskValue(of: first).error == .success)
        #expect(await actor.sharedFetches[account1] == nil)
        #expect(await actor.sharedFetches[account2] != nil)
        #expect(await actor.getCapabilities(for: account2) == nil)

        releaseSecondContinuation.yield(())
        #expect(try await testTaskValue(of: second).error == .success)
        #expect(await actor.sharedFetches.isEmpty)
        #expect(await actor.getCapabilities(for: account1)?.capabilities == capabilities)
        #expect(await actor.getCapabilities(for: account2)?.capabilities == capabilities)
    }
}
