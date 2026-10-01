//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Security
import Testing

///
/// Coverage for the queries of ``Keychain`` without touching any keychain.
///
/// Generic passwords ignore internet password attributes, so a query which contains one silently matches more items than intended.
///
struct KeychainQueryTests {
    private static let service = "B7A1F7E4-2C51-4F58-9A0B-3C6C2E2F1D10"
    private static let account = "alice"
    private static let password = Data("secret".utf8)

    private static let internetPasswordAttributes = [
        kSecAttrServer,
        kSecAttrProtocol,
        kSecAttrPort,
        kSecAttrPath,
        kSecAttrSecurityDomain,
        kSecAttrAuthenticationType
    ].map { $0 as String }

    @Test func itemQueryMatchesGenericPasswordByServiceAndAccount() throws {
        let query = try #require(Keychain.itemQuery(service: Self.service, account: Self.account))

        #expect(Set(query.keys) == [kSecClass as String, kSecAttrService as String, kSecAttrAccount as String])
        #expect(query[kSecClass as String] as? String == kSecClassGenericPassword as String)
        #expect(query[kSecAttrService as String] as? String == Self.service)
        #expect(query[kSecAttrAccount as String] as? String == Self.account)
    }

    @Test func noQueryUsesInternetPasswordAttributes() throws {
        let dictionaries = try [
            #require(Keychain.itemQuery(service: Self.service, account: Self.account)),
            #require(Keychain.lookupQuery(service: Self.service, account: Self.account)),
            #require(Keychain.addAttributes(service: Self.service, account: Self.account, label: "Label", password: Self.password)),
            Keychain.updateAttributes(label: "Label", password: Self.password),
            #require(Keychain.deleteQuery(service: Self.service))
        ]

        for dictionary in dictionaries {
            #expect(Set(dictionary.keys).isDisjoint(with: Self.internetPasswordAttributes))
        }
    }

    @Test func lookupQueryReturnsDataOfOneItem() throws {
        let query = try #require(Keychain.lookupQuery(service: Self.service, account: Self.account))

        #expect(query[kSecAttrService as String] as? String == Self.service)
        #expect(query[kSecAttrAccount as String] as? String == Self.account)
        #expect(query[kSecMatchLimit as String] as? String == kSecMatchLimitOne as String)
        #expect(query[kSecReturnData as String] as? Bool == true)
        // A renamed domain must still find its item.
        #expect(query[kSecAttrLabel as String] == nil)
    }

    @Test func addAttributesCarryLabelAndPassword() throws {
        let attributes = try #require(Keychain.addAttributes(service: Self.service, account: Self.account, label: "Label", password: Self.password))

        #expect(attributes[kSecAttrService as String] as? String == Self.service)
        #expect(attributes[kSecAttrAccount as String] as? String == Self.account)
        #expect(attributes[kSecAttrLabel as String] as? String == "Label")
        #expect(attributes[kSecValueData as String] as? Data == Self.password)
    }

    @Test func updateAttributesChangeOnlyLabelAndPassword() {
        let attributes = Keychain.updateAttributes(label: "Label", password: Self.password)

        #expect(Set(attributes.keys) == [kSecAttrLabel as String, kSecValueData as String])
    }

    @Test func deleteQueryCoversEveryAccountOfTheDomain() throws {
        let query = try #require(Keychain.deleteQuery(service: Self.service))

        #expect(Set(query.keys) == [kSecClass as String, kSecAttrService as String])
        #expect(query[kSecClass as String] as? String == kSecClassGenericPassword as String)
        #expect(query[kSecAttrService as String] as? String == Self.service)
    }

    @Test func emptyServiceProducesNoDeleteQuery() {
        #expect(Keychain.deleteQuery(service: "") == nil)
    }

    @Test(arguments: [("", "alice"), ("B7A1F7E4-2C51-4F58-9A0B-3C6C2E2F1D10", ""), ("", "")])
    func emptyServiceOrAccountProducesNoQuery(service: String, account: String) {
        #expect(Keychain.itemQuery(service: service, account: account) == nil)
        #expect(Keychain.lookupQuery(service: service, account: account) == nil)
        #expect(Keychain.addAttributes(service: service, account: account, label: "Label", password: Self.password) == nil)
    }

    @Test func keyComesFromTheDomain() {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(Self.service), displayName: "cloud.example.com - alice")
        let keychain = Keychain(domain: domain, log: FileProviderLogMock())

        #expect(keychain.service == Self.service)
        #expect(keychain.label == "cloud.example.com - alice")
    }
}
