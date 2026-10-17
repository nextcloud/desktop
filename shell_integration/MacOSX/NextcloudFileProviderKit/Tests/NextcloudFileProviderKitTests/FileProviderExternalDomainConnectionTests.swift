//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import Testing

struct FileProviderExternalDomainConnectionTests {
    @Test func configuredAccountConnects() {
        #expect(FileProviderExternalDomainConnection.shouldConnect(
            domainUserInfo: [FileProviderExternalDomainConnection.accountIdentifierUserInfoKey: "alice@example.com"],
            locallyConfiguredAccountIdentifiers: ["alice@example.com"]
        ))
    }

    @Test func unconfiguredAccountStaysDisconnected() {
        #expect(!FileProviderExternalDomainConnection.shouldConnect(
            domainUserInfo: [FileProviderExternalDomainConnection.accountIdentifierUserInfoKey: "bob@example.com"],
            locallyConfiguredAccountIdentifiers: ["alice@example.com"]
        ))
    }

    @Test func noConfiguredAccountsStaysDisconnected() {
        #expect(!FileProviderExternalDomainConnection.shouldConnect(
            domainUserInfo: [FileProviderExternalDomainConnection.accountIdentifierUserInfoKey: "alice@example.com"],
            locallyConfiguredAccountIdentifiers: []
        ))
    }

    @Test func domainWithoutAccountMappingStaysDisconnected() {
        #expect(!FileProviderExternalDomainConnection.shouldConnect(
            domainUserInfo: nil,
            locallyConfiguredAccountIdentifiers: ["alice@example.com"]
        ))
    }
}
