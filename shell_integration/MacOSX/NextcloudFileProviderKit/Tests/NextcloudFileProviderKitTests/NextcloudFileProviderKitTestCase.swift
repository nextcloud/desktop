//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import RealmSwift
import XCTest

///
/// Common base class for all tests in this target.
///
class NextcloudFileProviderKitTestCase: XCTestCase {
    private var retainedDatabase: Realm?

    override func setUp() async throws {
        try await super.setUp()
        // Tests reuse accounts while advertising different server capabilities.
        await RetrievedCapabilitiesActor.shared.reset()
    }

    func setUpDatabase(_ dbManager: FilesDatabaseManager) {
        // Retain the in-memory Realm so its contents survive task hops.
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        retainedDatabase = dbManager.ncDatabase()
    }

    override func tearDown() {
        retainedDatabase = nil
        super.tearDown()
    }

    ///
    /// Create a unique and temporary directory for Realm database testing purposes.
    ///
    /// - Returns: A URL pointing to a temporary directory which also contains a UUID to distinguish it clearly from any other calls.
    ///
    static func makeDatabaseDirectory() -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try! FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)

        return url
    }

    ///
    /// Instance wrapper for ``makeDatabaseDirectory`` for convenience and brevity.
    ///
    func makeDatabaseDirectory() -> URL {
        Self.makeDatabaseDirectory()
    }
}
