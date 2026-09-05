//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

///
/// The system launches the extension process and starts asking it for work before the main app has
/// handed the account over. Rejecting those requests outright loses them: observed on one extension
/// relaunch, 17 `fetchContents` calls arrived in the first 0.6 seconds — 1.7 seconds before the
/// account landed — and the framework never re-requested most of them.
///
/// ``FileProviderExtension/awaitAccount(timeoutNanoseconds:)`` turns that into a short wait.
///
/// None of these tests place an upper bound on elapsed time. A slow machine would fail such an
/// assertion while the code was perfectly correct. What distinguishes "returned at once" from
/// "waited, then returned" here is the *outcome*: a call given no time to wait can only produce an
/// account by taking the fast path, because parking would throw.
///
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

    ///
    /// An account already in place is returned without waiting at all.
    ///
    /// The timeout is zero, so the fast path is the only way this can return an account: were the
    /// early return removed, the call would park, the expired timer would release it, and it would
    /// throw. No clock is involved, so the result does not depend on how fast the machine is.
    ///
    @Test func returnsImmediatelyWhenAccountIsAlreadySetUp() async throws {
        let ext = makeExtension()
        ext.ncAccount = Self.account

        let account = try await ext.awaitAccount(timeoutNanoseconds: 0)

        #expect(account == Self.account)
    }

    ///
    /// The waiting case: a request arrives first, the account lands while it is parked, and the
    /// request proceeds instead of failing.
    ///
    /// The sleep only orders the two halves and cannot make this flake: if it is too short and the
    /// account is published first, the call takes the fast path and returns the same account.
    ///
    @Test func waitsForAnAccountThatArrivesLater() async throws {
        let ext = makeExtension()

        async let awaited = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)

        try await Task.sleep(for: .milliseconds(200))
        ext.ncAccount = Self.account
        ext.signalAccountReady()

        let account = try await awaited

        #expect(account == Self.account, "A request parked before setup must proceed once the account lands.")
    }

    ///
    /// A domain that never gets an account still fails — just after the timeout rather than
    /// instantly — so a genuinely unauthenticated domain cannot hang a request forever.
    ///
    /// The elapsed-time check is a lower bound, which a slow machine can only overshoot. It is what
    /// separates "waited and gave up" from "failed without waiting at all".
    ///
    @Test func givesUpAfterTheTimeoutWhenNoAccountArrives() async {
        let ext = makeExtension()
        let timeout = Duration.milliseconds(300)

        let started = ContinuousClock().now

        await #expect(throws: NSFileProviderError(.notAuthenticated)) {
            try await ext.awaitAccount(timeoutNanoseconds: 300_000_000)
        }

        #expect(ContinuousClock().now - started >= timeout)
    }

    ///
    /// Several parked requests are all released by the single setup completion.
    ///
    @Test func releasesEveryParkedWaiter() async throws {
        let ext = makeExtension()

        async let first = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)
        async let second = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)
        async let third = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)

        try await Task.sleep(for: .milliseconds(200))
        ext.ncAccount = Self.account
        ext.signalAccountReady()

        let results = try await [first, second, third]

        #expect(results == [Self.account, Self.account, Self.account])
    }
}
