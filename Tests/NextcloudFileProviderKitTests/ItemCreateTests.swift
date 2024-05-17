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

    lazy var rootItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
        name: "root",
        remotePath: Self.account.davFilesUrl,
        directory: true,
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        serverUrl: Self.account.serverUrl
    )
    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        rootItem.children = []
    }

    func testCreateFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let folderItemMetadata = ItemMetadata()
        folderItemMetadata.name = "folder"
        folderItemMetadata.fileName = "folder"
        folderItemMetadata.fileNameView = "folder"
        folderItemMetadata.directory = true
        folderItemMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        folderItemMetadata.serverUrl = Self.account.davFilesUrl

        let folderItemTemplate = Item(
            metadata: folderItemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )
        let (createdItemMaybe, error) = await Item.create(
            basedOn: folderItemTemplate,
            contents: nil,
            remoteInterface: remoteInterface,
            ncAccount: Self.account,
            progress: Progress(),
            dbManager: Self.dbManager
        )
        let createdItem = try XCTUnwrap(createdItemMaybe)

        XCTAssertNil(error)
        XCTAssertNotNil(createdItem)
        XCTAssertEqual(createdItem.metadata.fileName, folderItemMetadata.fileName)
        XCTAssertEqual(createdItem.metadata.directory, true)

        XCTAssertNotNil(rootItem.children.first { $0.name == folderItemMetadata.name })
        XCTAssertNotNil(
            rootItem.children.first { $0.identifier == createdItem.itemIdentifier.rawValue }
        )
        let remoteItem = rootItem.children.first { $0.name == folderItemMetadata.name }
        XCTAssertTrue(remoteItem?.directory ?? false)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, folderItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, folderItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, folderItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, folderItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdItem.itemIdentifier.rawValue)
    }

    func testCreateFile() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let fileItemMetadata = ItemMetadata()
        fileItemMetadata.fileName = "file"
        fileItemMetadata.fileNameView = "file"
        fileItemMetadata.directory = false
        fileItemMetadata.classFile = NKCommon.TypeClassFile.document.rawValue
        fileItemMetadata.serverUrl = Self.account.davFilesUrl

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        try Data("Hello world".utf8).write(to: tempUrl)

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )
        let (createdItemMaybe, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            remoteInterface: remoteInterface,
            ncAccount: Self.account,
            progress: Progress(),
            dbManager: Self.dbManager
        )
        let createdItem = try XCTUnwrap(createdItemMaybe)

        XCTAssertNil(error)
        XCTAssertNotNil(createdItem)
        XCTAssertEqual(createdItem.metadata.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(createdItem.metadata.directory, fileItemMetadata.directory)

        let remoteItem = try XCTUnwrap(
            rootItem.children.first { $0.identifier == createdItem.itemIdentifier.rawValue }
        )
        XCTAssertEqual(remoteItem.name, fileItemMetadata.fileName)
        XCTAssertEqual(remoteItem.directory, fileItemMetadata.directory)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, fileItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, fileItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, fileItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdItem.itemIdentifier.rawValue)
    }

    func testCreateFileIntoFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

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

        let (createdFolderItemMaybe, folderError) = await Item.create(
            basedOn: folderItemTemplate,
            contents: nil,
            remoteInterface: remoteInterface,
            ncAccount: Self.account,
            progress: Progress(),
            dbManager: Self.dbManager
        )

        XCTAssertNil(folderError)
        let createdFolderItem = try XCTUnwrap(createdFolderItemMaybe)

        let fileRelativeRemotePath = "/folder"
        let fileItemMetadata = ItemMetadata()
        fileItemMetadata.name = "file"
        fileItemMetadata.fileName = "file"
        fileItemMetadata.fileNameView = "file"
        fileItemMetadata.directory = false
        fileItemMetadata.serverUrl = Self.account.davFilesUrl + fileRelativeRemotePath
        fileItemMetadata.classFile = NKCommon.TypeClassFile.document.rawValue

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: createdFolderItem.itemIdentifier,
            remoteInterface: remoteInterface
        )

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        try Data("Hello world".utf8).write(to: tempUrl)

        let (createdFileItemMaybe, fileError) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            remoteInterface: remoteInterface,
            ncAccount: Self.account,
            progress: Progress(),
            dbManager: Self.dbManager
        )
        let createdFileItem = try XCTUnwrap(createdFileItemMaybe)

        XCTAssertNil(fileError)
        XCTAssertNotNil(createdFileItem)
        
        let remoteFolderItem = rootItem.children.first { $0.name == "folder" }
        XCTAssertNotNil(remoteFolderItem)
        XCTAssertFalse(remoteFolderItem?.children.isEmpty ?? true)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(createdFileItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, fileItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, fileItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, fileItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdFileItem.itemIdentifier.rawValue)

        let parentDbItem = try XCTUnwrap(
            Self.dbManager.itemMetadataFromOcId(createdFolderItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(parentDbItem.fileName, folderItemMetadata.fileName)
        XCTAssertEqual(parentDbItem.fileNameView, folderItemMetadata.fileNameView)
        XCTAssertEqual(parentDbItem.directory, folderItemMetadata.directory)
        XCTAssertEqual(parentDbItem.serverUrl, folderItemMetadata.serverUrl)
    }
}
