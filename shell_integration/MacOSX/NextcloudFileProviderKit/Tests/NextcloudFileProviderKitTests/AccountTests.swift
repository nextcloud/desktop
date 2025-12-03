//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import XCTest

final class AccountTests: NextcloudFileProviderKitTestCase {
    func testInitializationDirect() {
        let user = "user"
        let userId = "userId"
        let password = "password"
        let serverUrl = "https://example.com"
        let account = Account(user: user, id: userId, serverUrl: serverUrl, password: password)

        XCTAssertEqual(account.username, user)
        XCTAssertEqual(account.id, userId)
        XCTAssertEqual(account.password, password)
        XCTAssertEqual(account.ncKitAccount, "\(user) \(serverUrl)")
        XCTAssertEqual(account.serverUrl, serverUrl)
        XCTAssertEqual(account.davFilesUrl, serverUrl + Account.webDavFilesUrlSuffix + userId)
        XCTAssertEqual(account.trashUrl, serverUrl + Account.webDavTrashUrlSuffix + "\(userId)/trash")
        XCTAssertEqual(
            account.trashRestoreUrl, serverUrl + Account.webDavTrashUrlSuffix + "\(userId)/restore"
        )
        XCTAssertEqual(account.fileName, "\(userId)_example_com")
    }

    func testInitializationFromDictionary() {
        let dictionary: [String: String] = [
            AccountDictUsernameKey: "user",
            AccountDictIdKey: "userId",
            AccountDictPasswordKey: "password",
            AccountDictNcKitAccountKey: "user https://example.com",
            AccountDictServerUrlKey: "https://example.com",
            AccountDictDavFilesUrlKey: "https://example.com/remote.php/dav/files/user",
            AccountDictTrashUrlKey: "https://example.com/remote.php/dav/trashbin/user/trash",
            AccountDictTrashRestoreUrlKey: "https://example.com/remote.php/dav/trashbin/user/restore",
            AccountDictFileNameKey: "userId_example_com"
        ]

        let account = Account(dictionary: dictionary)

        XCTAssertNotNil(account)
        XCTAssertEqual(account?.username, "user")
        XCTAssertEqual(account?.id, "userId")
        XCTAssertEqual(account?.password, "password")
        XCTAssertEqual(account?.ncKitAccount, "user https://example.com")
        XCTAssertEqual(account?.serverUrl, "https://example.com")
        XCTAssertEqual(account?.davFilesUrl, "https://example.com/remote.php/dav/files/user")
        XCTAssertEqual(account?.trashUrl, "https://example.com/remote.php/dav/trashbin/user/trash")
        XCTAssertEqual(
            account?.trashRestoreUrl, "https://example.com/remote.php/dav/trashbin/user/restore"
        )
        XCTAssertEqual(account?.fileName, "userId_example_com")
    }

    func testInitializationFromIncompleteDictionary() {
        let incompleteDictionary: [String: String] = [
            AccountDictUsernameKey: "user"
            // missing other keys
        ]

        let account = Account(dictionary: incompleteDictionary)
        XCTAssertNil(account)
    }

    func testDictionaryRepresentation() {
        let account = Account(
            user: "user", id: "userId", serverUrl: "https://example.com", password: "password"
        )
        let dictionary = account.dictionary()

        XCTAssertEqual(dictionary[AccountDictUsernameKey], "user")
        XCTAssertEqual(dictionary[AccountDictPasswordKey], "password")
        XCTAssertEqual(dictionary[AccountDictIdKey], "userId")
        XCTAssertEqual(dictionary[AccountDictNcKitAccountKey], "user https://example.com")
        XCTAssertEqual(dictionary[AccountDictServerUrlKey], "https://example.com")
        XCTAssertEqual(dictionary[AccountDictDavFilesUrlKey], "https://example.com/remote.php/dav/files/userId")
        XCTAssertEqual(dictionary[AccountDictTrashUrlKey], "https://example.com/remote.php/dav/trashbin/userId/trash")
        XCTAssertEqual(dictionary[AccountDictTrashRestoreUrlKey], "https://example.com/remote.php/dav/trashbin/userId/restore")
        XCTAssertEqual(dictionary[AccountDictFileNameKey], "userId_example_com")
    }

    func testEquatability() {
        let account1 = Account(
            user: "user", id: "userId", serverUrl: "https://example.com", password: "password"
        )
        let account2 = Account(
            user: "user", id: "userId", serverUrl: "https://example.com", password: "password"
        )

        XCTAssertEqual(account1, account2)

        let account3 = Account(
            user: "user", id: "userId", serverUrl: "https://example.net", password: "password"
        )
        XCTAssertNotEqual(account1, account3)
    }

    func testFilenameValid() {
        let account = Account(
            user: "user", id: "/u/s/e.r/", serverUrl: "https://example.com", password: "password"
        )
        XCTAssertEqual(account.fileName, "u_s_e_r_example_com")
    }
}
