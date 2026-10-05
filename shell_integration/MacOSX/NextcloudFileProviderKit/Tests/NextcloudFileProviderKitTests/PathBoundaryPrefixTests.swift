//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
@testable import TestInterface
import XCTest

// MARK: - Path boundary prefix matching

final class PathBoundaryPrefixTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(
        account: account,
        databaseDirectory: makeDatabaseDirectory(),
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
        log: FileProviderLogMock()
    )

    override func setUp() {
        super.setUp()
        try! Self.dbManager.removeAllRowsForTesting()
    }

    func testChildItemsMatchesDirectChildButNotSiblingWithSharedPrefix() throws {
        var directoryMetadata = SendableItemMetadata.rawRow(ocId: "dir-A")
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "photos"
        directoryMetadata.directory = true

        var childMetadata = SendableItemMetadata.rawRow(ocId: "child-1")
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/photos"
        childMetadata.fileName = "pic.jpg"

        var nestedChild = SendableItemMetadata.rawRow(ocId: "nested-1")
        nestedChild.account = "TestAccount"
        nestedChild.serverUrl = "https://cloud.example.com/files/photos/vacation"
        nestedChild.fileName = "beach.jpg"

        var siblingMetadata = SendableItemMetadata.rawRow(ocId: "sibling-1")
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/photos-backup"
        siblingMetadata.fileName = "old.jpg"

        try Self.dbManager.insertForTesting(directoryMetadata)
        try Self.dbManager.insertForTesting(childMetadata)
        try Self.dbManager.insertForTesting(nestedChild)
        try Self.dbManager.insertForTesting(siblingMetadata)

        let children = Self.dbManager.childItems(directoryMetadata: directoryMetadata)
        let childOcIds = Set(children.map(\.ocId))
        XCTAssertTrue(childOcIds.contains("child-1"), "Direct child should be matched")
        XCTAssertTrue(childOcIds.contains("nested-1"), "Nested descendant should be matched")
        XCTAssertFalse(childOcIds.contains("sibling-1"), "Sibling with shared prefix must not match")
        XCTAssertEqual(children.count, 2)
    }

    func testChildItemCountExcludesSiblingWithSharedPrefix() throws {
        var directoryMetadata = SendableItemMetadata.rawRow(ocId: "dir-B")
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "docs"
        directoryMetadata.directory = true

        var childMetadata = SendableItemMetadata.rawRow(ocId: "child-2")
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/docs"
        childMetadata.fileName = "report.pdf"

        var siblingMetadata = SendableItemMetadata.rawRow(ocId: "sibling-2")
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/docs-archive"
        siblingMetadata.fileName = "old-report.pdf"

        try Self.dbManager.insertForTesting(directoryMetadata)
        try Self.dbManager.insertForTesting(childMetadata)
        try Self.dbManager.insertForTesting(siblingMetadata)

        let count = Self.dbManager.childItemCount(directoryMetadata: directoryMetadata)
        XCTAssertEqual(count, 1)
    }

    func testDeleteDirectoryDoesNotDeleteSiblingWithSharedPrefix() throws {
        var directoryMetadata = SendableItemMetadata.rawRow(ocId: "dir-C")
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "work"
        directoryMetadata.directory = true

        var childMetadata = SendableItemMetadata.rawRow(ocId: "child-3")
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/work"
        childMetadata.fileName = "task.txt"

        var siblingMetadata = SendableItemMetadata.rawRow(ocId: "sibling-3")
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/work-old"
        siblingMetadata.fileName = "task-old.txt"

        try Self.dbManager.insertForTesting(directoryMetadata)
        try Self.dbManager.insertForTesting(childMetadata)
        try Self.dbManager.insertForTesting(siblingMetadata)

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "dir-C")
        XCTAssertNotNil(deleted)
        XCTAssertEqual(deleted?.count, 2, "Should delete directory + direct child")

        let survivingSibling = Self.dbManager.itemMetadata(ocId: "sibling-3")
        XCTAssertNotNil(survivingSibling)
        XCTAssertFalse(survivingSibling?.deleted ?? true)
    }

    func testRenameDirectoryDoesNotRenameSiblingWithSharedPrefix() throws {
        var directoryMetadata = SendableItemMetadata.rawRow(ocId: "dir-D")
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "alpha"
        directoryMetadata.directory = true

        var childMetadata = SendableItemMetadata.rawRow(ocId: "child-4")
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/alpha"
        childMetadata.fileName = "file.txt"

        var siblingMetadata = SendableItemMetadata.rawRow(ocId: "sibling-4")
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/alphabet"
        siblingMetadata.fileName = "a.txt"

        try Self.dbManager.insertForTesting(directoryMetadata)
        try Self.dbManager.insertForTesting(childMetadata)
        try Self.dbManager.insertForTesting(siblingMetadata)

        let updated = Self.dbManager.renameDirectoryAndPropagateToChildren(
            ocId: "dir-D",
            newServerUrl: "https://cloud.example.com/files",
            newFileName: "beta"
        )

        XCTAssertNotNil(updated)
        XCTAssertTrue(
            updated?.contains(where: { $0.ocId == "child-4" }) ?? false,
            "Direct child should be in the updated results"
        )

        let renamedChild = Self.dbManager.itemMetadata(ocId: "child-4")
        XCTAssertEqual(
            renamedChild?.serverUrl,
            "https://cloud.example.com/files/beta",
            "Direct child's serverUrl should be updated"
        )

        let sibling = Self.dbManager.itemMetadata(ocId: "sibling-4")
        XCTAssertEqual(
            sibling?.serverUrl,
            "https://cloud.example.com/files/alphabet",
            "Sibling with shared prefix should not have its serverUrl changed"
        )
    }

    func testItemMetadatasUnderServerUrlExcludesSiblingPrefix() throws {
        var directChild = SendableItemMetadata.rawRow(ocId: "under-1")
        directChild.account = "TestAccount"
        directChild.serverUrl = "https://cloud.example.com/files/project"
        directChild.fileName = "readme.md"

        var nestedChild = SendableItemMetadata.rawRow(ocId: "under-2")
        nestedChild.account = "TestAccount"
        nestedChild.serverUrl = "https://cloud.example.com/files/project/src"
        nestedChild.fileName = "main.swift"

        var siblingMetadata = SendableItemMetadata.rawRow(ocId: "under-3")
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/project-v2"
        siblingMetadata.fileName = "readme.md"

        try Self.dbManager.insertForTesting(directChild)
        try Self.dbManager.insertForTesting(nestedChild)
        try Self.dbManager.insertForTesting(siblingMetadata)

        let results = Self.dbManager.itemMetadatas(
            account: "TestAccount",
            underServerUrl: "https://cloud.example.com/files/project"
        )
        let resultOcIds = Set(results.map(\.ocId))
        XCTAssertTrue(resultOcIds.contains("under-1"), "Direct child should be included")
        XCTAssertTrue(resultOcIds.contains("under-2"), "Nested child should be included")
        XCTAssertFalse(resultOcIds.contains("under-3"), "Sibling with shared prefix must not match")
        XCTAssertEqual(results.count, 2)
    }
}
