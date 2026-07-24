//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
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
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    func testChildItemsMatchesDirectChildButNotSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-A"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "photos")
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-1"
        childMetadata.account = "TestAccount"
        childMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/photos", fileName: "pic.jpg")

        let nestedChild = RealmItemMetadata()
        nestedChild.ocId = "nested-1"
        nestedChild.account = "TestAccount"
        nestedChild.updateLocation(serverUrl: "https://cloud.example.com/files/photos/vacation", fileName: "beach.jpg")

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-1"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/photos-backup", fileName: "old.jpg")

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(nestedChild)
            realm.add(siblingMetadata)
        }

        let children = Self.dbManager.childItems(
            directoryMetadata: SendableItemMetadata(value: directoryMetadata)
        )
        let childOcIds = Set(children.map(\.ocId))
        XCTAssertTrue(childOcIds.contains("child-1"), "Direct child should be matched")
        XCTAssertTrue(childOcIds.contains("nested-1"), "Nested descendant should be matched")
        XCTAssertFalse(childOcIds.contains("sibling-1"), "Sibling with shared prefix must not match")
        XCTAssertEqual(children.count, 2)
    }

    func testChildItemCountExcludesSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-B"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "docs")
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-2"
        childMetadata.account = "TestAccount"
        childMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/docs", fileName: "report.pdf")

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-2"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/docs-archive", fileName: "old-report.pdf")

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(siblingMetadata)
        }

        let count = Self.dbManager.childItemCount(
            directoryMetadata: SendableItemMetadata(value: directoryMetadata)
        )
        XCTAssertEqual(count, 1)
    }

    func testDeleteDirectoryDoesNotDeleteSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-C"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "work")
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-3"
        childMetadata.account = "TestAccount"
        childMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/work", fileName: "task.txt")

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-3"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/work-old", fileName: "task-old.txt")

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(siblingMetadata)
        }

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "dir-C")
        XCTAssertNotNil(deleted)
        XCTAssertEqual(deleted?.count, 2, "Should delete directory + direct child")

        let survivingSibling = Self.dbManager.itemMetadata(ocId: "sibling-3")
        XCTAssertNotNil(survivingSibling)
        XCTAssertFalse(survivingSibling?.deleted ?? true)
    }

    func testRenameDirectoryDoesNotRenameSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-D"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "alpha")
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-4"
        childMetadata.account = "TestAccount"
        childMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/alpha", fileName: "file.txt")

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-4"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/alphabet", fileName: "a.txt")

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(siblingMetadata)
        }

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
        let directChild = RealmItemMetadata()
        directChild.ocId = "under-1"
        directChild.account = "TestAccount"
        directChild.updateLocation(serverUrl: "https://cloud.example.com/files/project", fileName: "readme.md")

        let nestedChild = RealmItemMetadata()
        nestedChild.ocId = "under-2"
        nestedChild.account = "TestAccount"
        nestedChild.updateLocation(serverUrl: "https://cloud.example.com/files/project/src", fileName: "main.swift")

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "under-3"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.updateLocation(serverUrl: "https://cloud.example.com/files/project-v2", fileName: "readme.md")

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directChild)
            realm.add(nestedChild)
            realm.add(siblingMetadata)
        }

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
