//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import XCTest

///
/// The system launches the extension process and starts asking it for work before the main app has
/// handed the account over. Rejecting those requests outright loses them: observed on one extension
/// relaunch, 17 `fetchContents` calls arrived in the first 0.6 seconds — 1.7 seconds before the
/// account landed — and the framework never re-requested most of them.
///
/// ``FileProviderExtension/awaitAccount(timeoutNanoseconds:)`` turns that into a short wait.
///
final class AwaitAccountTests: NextcloudFileProviderKitTestCase {
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

    /// An account already in place is returned without waiting at all.
    func testReturnsImmediatelyWhenAccountIsAlreadySetUp() async {
        let ext = makeExtension()
        ext.ncAccount = Self.account

        let started = ContinuousClock().now
        let account = await ext.awaitAccount(timeoutNanoseconds: 5_000_000_000)

        XCTAssertEqual(account, Self.account)
        XCTAssertLessThan(ContinuousClock().now - started, .milliseconds(500))
    }

    /// The waiting case: a request arrives first, the account lands while it is parked, and the
    /// request proceeds instead of failing.
    func testWaitsForAnAccountThatArrivesLater() async {
        let ext = makeExtension()

        async let awaited = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)

        // Let the caller park before the account is published.
        try? await Task.sleep(nanoseconds: 200_000_000)
        ext.ncAccount = Self.account
        ext.signalAccountReady()

        let account = await awaited
        XCTAssertEqual(account, Self.account, "A request parked before setup must proceed once the account lands.")
    }

    /// A domain that never gets an account still fails — just after the timeout rather than
    /// instantly — so a genuinely unauthenticated domain cannot hang a request forever.
    func testGivesUpAfterTheTimeoutWhenNoAccountArrives() async {
        let ext = makeExtension()

        let started = ContinuousClock().now
        let account = await ext.awaitAccount(timeoutNanoseconds: 300_000_000)
        let elapsed = ContinuousClock().now - started

        XCTAssertNil(account)
        XCTAssertGreaterThan(elapsed, .milliseconds(200))
        XCTAssertLessThan(elapsed, .seconds(5))
    }

    /// Several parked requests are all released by the single setup completion.
    func testReleasesEveryParkedWaiter() async {
        let ext = makeExtension()

        async let first = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)
        async let second = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)
        async let third = ext.awaitAccount(timeoutNanoseconds: 10_000_000_000)

        try? await Task.sleep(nanoseconds: 200_000_000)
        ext.ncAccount = Self.account
        ext.signalAccountReady()

        let results = await [first, second, third]
        XCTAssertEqual(results, [Self.account, Self.account, Self.account])
    }
}
