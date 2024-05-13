//
//  ItemCreateTests.swift
//
//
//  Created by Claudio Cambra on 13/5/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

final class ItemCreateTests: XCTestCase {
    static let account = Account(
        user: "testUser", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    static let rootItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
        name: "root",
        directory: true
    )
    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Bundle.main.object(
            forInfoDictionaryKey: "NCFPKAppGroupIdentifier"
        )
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        Self.rootItem.children = []
    }

    func testCreateFolder() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let folderItemMetadata = ItemMetadata()
        folderItemMetadata.name = "folder"
        folderItemMetadata.fileName = "folder"
        folderItemMetadata.fileNameView = "folder"
        folderItemMetadata.directory = true
        folderItemMetadata.serverUrl = Self.account.davFilesUrl
        folderItemMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue

        let folderItemTemplate = Item(
            metadata: folderItemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )
        let (createdItem, error) = await Item.create(
            basedOn: folderItemTemplate,
            contents: nil,
            remoteInterface: remoteInterface,
            ncAccount: Self.account,
            progress: Progress(),
            dbManager: Self.dbManager
        )

        XCTAssertNil(error)
        XCTAssertNotNil(createdItem)
        XCTAssertEqual(createdItem?.metadata.fileName, "folder")
        XCTAssertEqual(createdItem?.metadata.directory, true)

        XCTAssertNotNil(Self.rootItem.children.first { $0.name == "folder" })
        XCTAssertNotNil(
            Self.rootItem.children.first { $0.identifier == createdItem?.itemIdentifier.rawValue }
        )
        let remoteItem = Self.rootItem.children.first { $0.name == "folder" }
        XCTAssertTrue(remoteItem?.directory ?? false)
    }

    func testCreateFile() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let fileItemMetadata = ItemMetadata()
        fileItemMetadata.name = "file"
        fileItemMetadata.fileName = "file"
        fileItemMetadata.fileNameView = "file"
        fileItemMetadata.directory = false
        fileItemMetadata.serverUrl = Self.account.davFilesUrl
        fileItemMetadata.classFile = NKCommon.TypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        try Data("Hello world".utf8).write(to: tempUrl)

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )
        let (createdItem, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            remoteInterface: remoteInterface,
            ncAccount: Self.account,
            progress: Progress(),
            dbManager: Self.dbManager
        )

        XCTAssertNil(error)
        XCTAssertNotNil(createdItem)
        XCTAssertEqual(createdItem?.metadata.fileName, "file")
        XCTAssertEqual(createdItem?.metadata.directory, false)
        XCTAssertNotNil(Self.rootItem.children.first { $0.name == "file" })
        XCTAssertNotNil(
            Self.rootItem.children.first { $0.identifier == createdItem?.itemIdentifier.rawValue }
        )
        let remoteItem = Self.rootItem.children.first { $0.name == "file" }
        XCTAssertFalse(remoteItem?.directory ?? true)
    }
}
