//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import XCTest

///
/// Common base class for all tests in this target.
///
class NextcloudFileProviderKitTestCase: XCTestCase {
    override func setUp() {
        super.setUp()

        // `PendingMaterializationRegistry` is a process-wide singleton, so entries recorded by a
        // download in one test would otherwise suppress the eviction reconciliation in another.
        PendingMaterializationRegistry.shared.removeAll()
        // Likewise process-wide: targets recorded by one test must not steer another's scan, and a
        // stale "last full scan" timestamp would silently turn a full walk into a targeted one.
        RemoteChangeTargets.shared.removeAll()
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
