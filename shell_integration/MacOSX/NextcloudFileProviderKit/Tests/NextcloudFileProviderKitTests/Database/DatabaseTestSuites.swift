//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

///
/// Umbrella for the Swift Testing suites which exercise ``FilesDatabaseManager`` directly.
///
/// Nested suites inherit the serialization. Every test still gets its own database in its own directory; the serialization only keeps tests from configuring the shared engine defaults at the same time.
///
@Suite("Database", .serialized)
enum DatabaseTestSuites {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    ///
    /// A database manager on a fresh temporary directory, so one test cannot see another's rows.
    ///
    static func makeManager(account: Account = account) -> FilesDatabaseManager {
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("DatabaseTestSuites-\(UUID().uuidString)", isDirectory: true)
        try! FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)

        return FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test-\(UUID().uuidString)"),
            log: FileProviderLogMock()
        )
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
