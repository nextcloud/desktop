//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import XCTest

/// Coverage for ``FilesDatabaseManager/setMaterializationStates(_:)``, which must not write back anything but the flags it is given.
final class FilesDatabaseManagerMaterializationStateTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var manager: FilesDatabaseManager!

    override func setUp() {
        super.setUp()
        manager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    func testWritesOnlyTheGivenFlags() throws {
        var file = SendableItemMetadata(ocId: "file", fileName: "report.pdf", account: Self.account)
        file.status = Status.downloading.rawValue
        file.etag = "etag-during-download"
        var directory = SendableItemMetadata(ocId: "directory", fileName: "docs", account: Self.account)
        directory.directory = true
        directory.downloaded = true
        try manager.insertForTesting(file)
        try manager.insertForTesting(directory)

        manager.setMaterializationStates([
            (ocId: "file", downloaded: true, visitedDirectory: nil),
            (ocId: "directory", downloaded: nil, visitedDirectory: true)
        ])

        let storedFile = try XCTUnwrap(manager.itemMetadata(ocId: "file"))
        XCTAssertTrue(storedFile.downloaded)
        XCTAssertFalse(storedFile.visitedDirectory)
        XCTAssertEqual(storedFile.status, Status.downloading.rawValue, "A status written since the pass read the row must survive.")
        XCTAssertEqual(storedFile.etag, "etag-during-download")

        let storedDirectory = try XCTUnwrap(manager.itemMetadata(ocId: "directory"))
        XCTAssertTrue(storedDirectory.visitedDirectory)
        XCTAssertTrue(storedDirectory.downloaded, "A flag the pass did not change must not be written back.")
    }

    func testLeavesAMissingRowAbsent() {
        manager.setMaterializationStates([(ocId: "deleted-meanwhile", downloaded: false, visitedDirectory: false)])

        XCTAssertNil(manager.itemMetadata(ocId: "deleted-meanwhile"))
    }
}
