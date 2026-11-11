//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

///
/// Coverage for `awaitDatabaseManager`, which waits for account setup instead of failing the requests the
/// framework makes before the main app has handed the account over.
///
@Suite(.timeLimit(.minutes(1)))
struct AwaitAccountTests {
    private static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private func makeExtension() -> FileProviderExtension {
        let domain = NSFileProviderDomain(
            identifier: NSFileProviderDomainIdentifier("test-domain-await-account"),
            displayName: "Test"
        )
        return FileProviderExtension(domain: domain)
    }

    private func waitForAccountWaiter(in ext: FileProviderExtension) async throws {
        let clock = ContinuousClock()
        let deadline = clock.now + asyncTestTimeout
        while ext.accountReadyLock.withLock({ ext.accountReadyWaiters.isEmpty }) {
            try Task.checkCancellation()
            guard clock.now < deadline else { throw URLError(.timedOut) }
            await Task.yield()
        }
    }

    @Test func cancelledWaiterDoesNotWaitForAccountTimeout() async throws {
        let ext = makeExtension()
        let (waiting, continuation) = AsyncStream<Void>.makeStream()
        let task = Task<Bool, Never> {
            do {
                _ = try await ext.awaitDatabaseManager(timeoutNanoseconds: UInt64.max) {
                    continuation.yield(())
                }
                return false
            } catch {
                return error is CancellationError
            }
        }
        defer {
            task.cancel()
            ext.signalAccountReady()
            ext.invalidate()
            continuation.finish()
        }
        try #require(try await nextTestValue(from: waiting) != nil)
        task.cancel()
        #expect(try await testTaskValue(of: task))
        #expect(ext.accountReadyLock.withLock { ext.accountReadyWaiters.isEmpty })
    }

    @Test func cancelledContentFetchFinishesBeforeAccountSetup() async throws {
        let ext = makeExtension()
        defer {
            // Release a stranded waiter if the cancellation bridge regresses.
            ext.signalAccountReady()
            ext.invalidate()
        }
        let (completion, completionContinuation) = AsyncStream<Error?>.makeStream()
        defer { completionContinuation.finish() }

        try await confirmation("Content fetch completes once", expectedCount: 1) { completed in
            let progress = ext.fetchContents(for: .rootContainer, version: nil, request: NSFileProviderRequest()) { url, item, error in
                #expect(url == nil)
                #expect(item == nil)
                completed()
                completionContinuation.yield(error)
                completionContinuation.finish()
            }
            defer { progress.cancel() }
            try await waitForAccountWaiter(in: ext)
            #expect(ext.dbManager == nil)
            progress.cancel()

            // The five-second test bound expires before awaitDatabaseManager's ten-second timeout.
            let error = try #require(try await nextTestValue(from: completion))
            #expect((error as? CocoaError)?.code == .userCancelled)
            #expect(ext.dbManager == nil)
            #expect(progress.cancellationHandler == nil)
            #expect(ext.accountReadyLock.withLock { ext.accountReadyWaiters.isEmpty })
            #expect(ext.actionsLock.withLock { ext.syncActions.isEmpty && ext.errorActions.isEmpty })
        }
    }

    ///
    /// A published database manager is available without waiting.
    ///
    @Test func returnsImmediatelyWhenAccountIsAlreadySetUp() async throws {
        let ext = makeExtension()
        let manager = DatabaseTestSuites.makeManager(account: Self.account)
        try #require(ext.publishDatabaseManager(manager, generation: ext.setupGeneration))

        let publishedManager = try await ext.awaitDatabaseManager(timeoutNanoseconds: 0)

        #expect(publishedManager === manager)
    }

    ///
    /// A request that arrives before setup waits for the database manager and keeps that
    /// manager's account if a later setup publishes a replacement.
    ///
    @Test func waitsForAnAccountThatArrivesLater() async throws {
        let ext = makeExtension()
        let manager = DatabaseTestSuites.makeManager(account: Self.account)

        async let awaited = ext.awaitDatabaseManager(timeoutNanoseconds: 10_000_000_000)

        try await waitForAccountWaiter(in: ext)
        try #require(ext.publishDatabaseManager(manager, generation: ext.setupGeneration))
        ext.signalAccountReady()

        let capturedManager = try await awaited
        let newAccount = Account(
            user: Self.account.username,
            id: "new-backend-user-id",
            serverUrl: Self.account.serverUrl,
            password: Self.account.password
        )
        let newManager = DatabaseTestSuites.makeManager(account: newAccount)
        try #require(ext.publishDatabaseManager(newManager, generation: ext.setupGeneration))

        #expect(capturedManager === manager)
        #expect(capturedManager.account == Self.account)
        #expect(ext.dbManager === newManager)
        #expect(ext.dbManager?.account == newAccount)
    }

    ///
    /// A domain that never gets an account fails after the timeout rather than hanging forever.
    ///
    @Test func givesUpAfterTheTimeoutWhenNoAccountArrives() async {
        let ext = makeExtension()
        let timeout = Duration.milliseconds(300)

        let started = ContinuousClock().now

        await #expect(throws: NSFileProviderError(.notAuthenticated)) {
            try await ext.awaitDatabaseManager(timeoutNanoseconds: 300_000_000)
        }

        #expect(ContinuousClock().now - started >= timeout)
    }

    ///
    /// Several parked requests are all released by the single setup completion.
    ///
    @Test func releasesEveryParkedWaiter() async throws {
        let ext = makeExtension()
        let manager = DatabaseTestSuites.makeManager(account: Self.account)

        async let first = ext.awaitDatabaseManager(timeoutNanoseconds: 10_000_000_000)
        async let second = ext.awaitDatabaseManager(timeoutNanoseconds: 10_000_000_000)
        async let third = ext.awaitDatabaseManager(timeoutNanoseconds: 10_000_000_000)

        try await Task.sleep(for: .milliseconds(200))
        try #require(ext.publishDatabaseManager(manager, generation: ext.setupGeneration))
        ext.signalAccountReady()

        let results = try await [first, second, third]

        #expect(results.allSatisfy { $0 === manager })
    }
}
