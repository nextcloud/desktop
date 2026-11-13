//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing
import TestInterface

///
/// Umbrella for the Swift Testing suites which exercise ``FilesDatabaseManager`` directly, with the fixtures they share.
///
/// Every test gets its own database in its own directory.
///
@Suite("Database")
enum DatabaseTestSuites {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    ///
    /// The defaults suite the managers made here record their store version in, kept out of the standard defaults of the test host.
    ///
    static var defaults: UserDefaults {
        UserDefaults(suiteName: "com.nextcloud.NextcloudFileProviderKitTests.DatabaseTestSuites")!
    }

    ///
    /// A database manager on a fresh temporary directory, so one test cannot see another's rows.
    ///
    static func makeManager(account: Account = account) -> FilesDatabaseManager {
        try! TemporaryDatabase(account: account, defaults: defaults).manager
    }

    ///
    /// A file row below the account's files root.
    ///
    static func makeFile(ocId: String, fileName: String, serverUrl: String? = nil, account: Account = account) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: fileName, account: account)
        metadata.serverUrl = serverUrl ?? account.davFilesUrl
        return metadata
    }
}
