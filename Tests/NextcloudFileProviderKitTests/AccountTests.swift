//
//  AccountTests.swift
//  
//
//  Created by Claudio Cambra on 17/5/24.
//

import Foundation
import XCTest
@testable import NextcloudFileProviderKit

final class AccountTests: XCTestCase {
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
            AccountDictTrashRestoreUrlKey: "https://example.com/remote.php/dav/trashbin/user/restore"
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
}
