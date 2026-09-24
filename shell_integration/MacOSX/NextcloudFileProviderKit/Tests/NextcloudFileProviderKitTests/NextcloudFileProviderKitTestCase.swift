//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import XCTest

///
/// Common base class for all tests in this target.
///
class NextcloudFileProviderKitTestCase: XCTestCase {
    ///
    /// Whether this test expects its production code to log `.error` or `.fault` messages.
    ///
    /// Left `false`, ``tearDown()`` fails the test when anything was logged at those levels. Several
    /// code paths log and carry on where they cannot throw, so without this a test can fail on a
    /// downstream assertion — or worse, pass — while the real complaint sits invisible in a log nobody
    /// reads. Override and return `true` in a test class that deliberately exercises failure paths.
    ///
    var expectsLoggedErrors: Bool {
        false
    }

    private var testDrivesAFailurePath = false

    ///
    /// Declare that *this* test drives a failure path, so the `.error` messages its production code
    /// logs are the point of the test rather than a defect.
    ///
    /// Prefer this to overriding ``expectsLoggedErrors``: it keeps the check live for every other test
    /// in the same class, and it documents at the top of the test that failure handling is what is
    /// under test.
    ///
    func expectLoggedErrors() {
        testDrivesAFailurePath = true
    }

    ///
    /// The database manager this test class runs against, if it has one built before ``setUp()``.
    ///
    /// ``setUp()`` needs it to put the in-memory Realm in place correctly; see there for why. Classes
    /// that build their manager later — inside a test, or from a per-test directory — leave this `nil`
    /// and get no in-memory store.
    ///
    var testDatabaseManager: FilesDatabaseManager? {
        nil
    }

    ///
    /// Retains the in-memory Realm for the whole test. Without a live reference the store is freed
    /// once a write returns, and the next access reopens an empty database.
    ///
    private var keepAliveRealm: Realm?

    override func setUp() {
        super.setUp()
        FileProviderLogProblemRecorder.reset()

        // Order matters, and not obviously. `FilesDatabaseManager.init` assigns
        // `Realm.Configuration.defaultConfiguration` wholesale with a *file-based* configuration, so
        // forcing the manager to initialise first is what stops it wiping the in-memory identifier set
        // straight after. Done the other way round — which is what happens when a class sets the
        // identifier itself and then touches a lazily initialised `static let dbManager` — the class
        // silently runs against `test.realm` on disk instead, and the first test of every class did
        // exactly that before this moved here.
        // Classes that declare no manager are left entirely alone: some of them run against whatever
        // configuration their own code puts in place, and setting an in-memory identifier for them
        // here changes which store they use.
        guard let manager = testDatabaseManager else {
            return
        }

        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        keepAliveRealm = manager.ncDatabase()

        XCTAssertEqual(
            Realm.Configuration.defaultConfiguration.inMemoryIdentifier,
            name,
            "This test must run against an in-memory Realm, not a file on disk."
        )
        XCTAssertNil(
            Realm.Configuration.defaultConfiguration.fileURL,
            "An in-memory Realm configuration carries no file URL; a URL here means a manager replaced the configuration after it was set."
        )
    }

    override func tearDown() {
        keepAliveRealm = nil
        super.tearDown()

        guard expectsLoggedErrors == false, testDrivesAFailurePath == false else {
            return
        }

        let problems = FileProviderLogProblemRecorder.problems

        if problems.isEmpty == false {
            XCTFail(
                """
                Production code logged \(problems.count) error(s) during this test:
                \(problems.map(\.description).joined(separator: "\n"))
                """
            )
        }
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
