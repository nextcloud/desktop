//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

///
/// Coverage for ``Keychain`` against the real default keychain, which is the only place where macOS ignores internet password attributes on generic passwords.
///
/// Each test uses random services and a random account, plants a decoy item of another "application" with the same account, and removes everything again afterwards.
///
@Suite(
    "Keychain in the default keychain",
    .enabled(
        if: LoginKeychainFixture.isUsable() || LoginKeychainFixture.isRequired,
        "The default keychain is not usable. Set NEXTCLOUD_REQUIRE_LOGIN_KEYCHAIN_TESTS=1 to fail instead of skipping."
    ),
    .timeLimit(.minutes(1))
)
final class KeychainLoginKeychainTests: Sendable {
    let account = "nextcloud-test-\(UUID().uuidString)"
    let renamedAccount = "nextcloud-test-\(UUID().uuidString)"
    let service = UUID().uuidString
    let otherService = UUID().uuidString
    let decoyService = "nextcloud-test-decoy-\(UUID().uuidString)"

    init() throws {
        try #require(LoginKeychainFixture.isUsable(), "The default keychain is not usable.")
    }

    deinit {
        for service in [service, otherService, decoyService] {
            LoginKeychainFixture.removeAll(service: service)
        }

        // Also catches items stored without a service, as the code before the fix did.
        for account in [account, renamedAccount] {
            LoginKeychainFixture.removeAll(account: account)
        }
    }

    private func makeKeychain(service: String, label: String = LoginKeychainFixture.label) -> Keychain {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: label)

        return Keychain(domain: domain, log: FileProviderLogMock())
    }

    private func plantDecoy() throws {
        try #require(LoginKeychainFixture.plant(service: decoyService, account: account, password: "decoy-secret", label: "decoy") == errSecSuccess)
    }

    @Test func savedPasswordReadsBackAndUpdatesInPlace() {
        let keychain = makeKeychain(service: service)

        #expect(keychain.savePassword("one", for: account))
        #expect(keychain.getPassword(for: account) == "one")
        #expect(keychain.savePassword("two", for: account))
        #expect(keychain.getPassword(for: account) == "two")
        #expect(LoginKeychainFixture.count(service: service) == 1)
    }

    @Test func savingLeavesForeignItemWithSameAccountUntouched() throws {
        try plantDecoy()
        let keychain = makeKeychain(service: service)

        #expect(keychain.savePassword("mine", for: account))
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
        #expect(LoginKeychainFixture.label(service: decoyService, account: account) == "decoy")
        #expect(LoginKeychainFixture.count(service: decoyService) == 1)
        #expect(LoginKeychainFixture.count(service: service) == 1)
    }

    @Test func lookupNeverReturnsForeignItemWithSameAccount() throws {
        try plantDecoy()
        let keychain = makeKeychain(service: service)

        #expect(keychain.getPassword(for: account) == nil)
    }

    @Test func domainsWithTheSameUserNameKeepSeparatePasswords() {
        let keychain = makeKeychain(service: service)
        let otherKeychain = makeKeychain(service: otherService)

        #expect(keychain.savePassword("a", for: account))
        #expect(otherKeychain.savePassword("b", for: account))
        #expect(keychain.getPassword(for: account) == "a")
        #expect(otherKeychain.getPassword(for: account) == "b")
        #expect(LoginKeychainFixture.count(service: service) == 1)
        #expect(LoginKeychainFixture.count(service: otherService) == 1)
    }

    @Test func labelFollowsTheDisplayNameOnUpdate() {
        #expect(makeKeychain(service: service, label: "Old").savePassword("one", for: account))
        #expect(makeKeychain(service: service, label: "New").savePassword("two", for: account))
        #expect(LoginKeychainFixture.label(service: service, account: account) == "New")
        #expect(LoginKeychainFixture.count(service: service) == 1)
    }

    @Test func deletingRemovesOnlyItemsOfTheOwnDomain() throws {
        try plantDecoy()
        let keychain = makeKeychain(service: service)
        let otherKeychain = makeKeychain(service: otherService)
        #expect(keychain.savePassword("a", for: account))
        #expect(otherKeychain.savePassword("b", for: account))

        #expect(keychain.deletePassword())
        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(otherKeychain.getPassword(for: account) == "b")
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
    }

    @Test func deletingRemovesItemsOfAnEarlierUserName() {
        let keychain = makeKeychain(service: service)
        #expect(keychain.savePassword("old", for: renamedAccount))
        #expect(keychain.savePassword("new", for: account))

        #expect(keychain.deletePassword())
        #expect(LoginKeychainFixture.count(service: service) == 0)
    }

    @Test func deletingWithNothingStoredSucceeds() {
        #expect(makeKeychain(service: service).deletePassword())
    }

    @Test func removingAccountConfigDeletesTheDomainPassword() throws {
        try plantDecoy()
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(service), displayName: LoginKeychainFixture.label)
        let ext = FileProviderExtension(domain: domain)
        ext.ncAccount = Account(user: account, id: account, serverUrl: "https://mock.nc.com", password: "x")
        #expect(ext.keychain.savePassword("x", for: account))

        ext.removeAccountConfig()

        #expect(LoginKeychainFixture.count(service: service) == 0)
        #expect(ext.ncAccount == nil)
        #expect(LoginKeychainFixture.password(service: decoyService, account: account) == "decoy-secret")
    }

    @Test func emptyPasswordIsNotSaved() {
        let keychain = makeKeychain(service: service)

        #expect(keychain.savePassword("", for: account) == false)
        #expect(LoginKeychainFixture.count(service: service) == 0)
    }
}
