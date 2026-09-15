//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

///
/// Coverage for `awaitAccount`, which waits for account setup instead of failing the requests the
/// framework makes before the main app has handed the account over.
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
    /// A domain that never gets an account fails after the timeout rather than hanging forever.
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
