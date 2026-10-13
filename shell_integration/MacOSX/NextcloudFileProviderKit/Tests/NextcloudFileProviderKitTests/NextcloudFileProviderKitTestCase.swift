//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import RealmSwift
import XCTest

///
/// Common base class for all tests in this target.
///
class NextcloudFileProviderKitTestCase: XCTestCase {
    private var retainedRealms: [Realm] = []

    override func tearDown() {
        retainedRealms.removeAll()
        super.tearDown()
    }

    /// Clears the test database and keeps in-memory records alive until teardown.
    func setUpDatabase(_ dbManager: FilesDatabaseManager) {
        let realm = dbManager.ncDatabase()
        do {
            try realm.write { realm.deleteAll() }
        } catch {
            XCTFail("Could not reset test database: \(error)")
        }
        retainedRealms.append(realm)
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
