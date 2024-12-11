//
//  ItemModifyTests.swift
//
//
//  Created by Claudio Cambra on 13/5/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import TestInterface
import UniformTypeIdentifiers
import XCTest
@testable import NextcloudFileProviderKit

final class ItemModifyTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    lazy var rootItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
        name: "root",
        remotePath: Self.account.davFilesUrl,
        directory: true,
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        userId: Self.account.id,
        serverUrl: Self.account.serverUrl
    )
    lazy var rootTrashItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.trashContainer.rawValue,
        name: "root",
        remotePath: Self.account.trashUrl,
        directory: true,
        account: Self.account.ncKitAccount,
        username: Self.account.username,
        userId: Self.account.id,
        serverUrl: Self.account.serverUrl
    )

    var remoteFolder: MockRemoteItem!
    var remoteItem: MockRemoteItem!

    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name

        remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: Self.account.davFilesUrl + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children = [remoteItem, remoteFolder]
        rootTrashItem.children = []
        remoteItem.parent = rootItem
        remoteFolder.parent = rootItem
    }

    func testModifyFileContents() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let targetItemMetadata = ItemMetadata(value: itemMetadata)
        targetItemMetadata.name = "item-renamed.txt" // Renamed
        targetItemMetadata.fileName = "item-renamed.txt" // Renamed
        targetItemMetadata.fileNameView = "item-renamed.txt" // Renamed
        targetItemMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move
        targetItemMetadata.date = .init()

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        let targetItem = Item(
            metadata: targetItemMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface
        )
        targetItem.dbManager = Self.dbManager

        let newContents = "Hello, New World!".data(using: .utf8)
        let newContentsUrl = FileManager.default.temporaryDirectory.appendingPathComponent("test")
        try newContents?.write(to: newContentsUrl)

        let (modifiedItemMaybe, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.filename, .contents, .parentItemIdentifier, .contentModificationDate],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedItem = try XCTUnwrap(modifiedItemMaybe)

        XCTAssertEqual(modifiedItem.itemIdentifier, targetItem.itemIdentifier)
        XCTAssertEqual(modifiedItem.filename, targetItem.filename)
        XCTAssertEqual(modifiedItem.parentItemIdentifier, targetItem.parentItemIdentifier)
        XCTAssertEqual(modifiedItem.contentModificationDate, targetItem.contentModificationDate)

        XCTAssertFalse(remoteFolder.children.isEmpty)
        XCTAssertEqual(remoteItem.data, newContents)
        XCTAssertEqual(remoteItem.name, targetItemMetadata.fileName)
        XCTAssertEqual(
            remoteItem.remotePath, targetItemMetadata.serverUrl + "/" + targetItemMetadata.fileName
        )
    }

    func testModifyBundleContents() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)

        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)

        let keynoteBundleFilename = "test.key"
        let keynoteIndexZipFilename = "Index.zip"
        let keynoteRandomFileName = "random.txt"
        let keynoteDataFolderName = "Data"
        let keynoteDataRandomImageName = "random.jpg"
        let keynoteMetadataFolderName = "Metadata"
        let keynoteDocIdentifierFilename = "DocumentIdentifier"
        let keynoteVersionPlistFilename = "BuildVersionHistory.plist"
        let keynotePropertiesPlistFilename = "Properties.plist"

        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            versionIdentifier: "old",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteBundle = MockRemoteItem(
            identifier: keynoteBundleFilename,
            versionIdentifier: "old",
            name: keynoteBundleFilename,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteDataFolder = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteDataFolderName,
            versionIdentifier: "old",
            name: keynoteDataFolderName,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteDataFolderName,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteDataRandomFile = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteDataFolderName + "/" + keynoteDataRandomImageName,
            versionIdentifier: "old",
            name: keynoteDataRandomImageName,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteDataFolderName + "/" + keynoteDataRandomImageName,
            data: "000".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteMetadataFolder = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteMetadataFolderName,
            versionIdentifier: "old",
            name: keynoteMetadataFolderName,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteMetadataFolderName,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteIndexZip = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteIndexZipFilename,
            versionIdentifier: "old",
            name: keynoteIndexZipFilename,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteIndexZipFilename,
            data: "This is a fake zip, pre modification".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteRandomFile = MockRemoteItem( // We will want this to be gone later
            identifier: keynoteBundleFilename + "/" + keynoteRandomFileName,
            versionIdentifier: "old",
            name: keynoteRandomFileName,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteRandomFileName,
            data: "This is a random file, I should be gone post modify".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteDocIdentifier = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteMetadataFolderName + "/" + keynoteDocIdentifierFilename,
            versionIdentifier: "old",
            name: keynoteDocIdentifierFilename,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteMetadataFolderName + "/" + keynoteDocIdentifierFilename,
            data: "8B0C6C1F-4DA4-4DE8-8510-0C91FDCE7D01".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteVersionPlist = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteMetadataFolderName + "/" + keynoteVersionPlistFilename,
            versionIdentifier: "new",
            name: keynoteVersionPlistFilename,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteMetadataFolderName + "/" + keynoteVersionPlistFilename,
            data: """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <string>Template: 35_DynamicWavesDark (14.1)</string>
    <string>M14.1-7040.0.73-4</string>
</array>
</plist>
""".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynotePropertiesPlist = MockRemoteItem(
            identifier: keynoteBundleFilename + "/" + keynoteMetadataFolderName + "/" + keynotePropertiesPlistFilename,
            versionIdentifier: "old",
            name: "Properties.plist",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/" + keynoteMetadataFolderName + "/" + keynotePropertiesPlistFilename,
            data: """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>revision</key>
    <string>0::5B42B84E-6F62-4E53-9E71-7DD24FA7E2EA</string>
    <key>documentUUID</key>
    <string>8B0C6C1F-4DA4-4DE8-8510-0C91FDCE7D01</string>
    <key>versionUUID</key>
    <string>5B42B84E-6F62-4E53-9E71-7DD24FA7E2EA</string>
    <key>privateUUID</key>
    <string>637C846B-6146-40C2-8EF8-26996E598E49</string>
    <key>isMultiPage</key>
    <false/>
    <key>stableDocumentUUID</key>
    <string>8B0C6C1F-4DA4-4DE8-8510-0C91FDCE7D01</string>
    <key>fileFormatVersion</key>
    <string>14.1.1</string>
    <key>shareUUID</key>
    <string>8B0C6C1F-4DA4-4DE8-8510-0C91FDCE7D01</string>
</dict>
</plist>
""".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children.forEach { $0.parent = nil }
        rootItem.children = [remoteKeynoteBundle, remoteFolder]
        remoteFolder.parent = rootItem
        remoteKeynoteBundle.parent = rootItem
        remoteKeynoteBundle.children = [
            remoteKeynoteIndexZip,
            remoteKeynoteRandomFile,
            remoteKeynoteDataFolder,
            remoteKeynoteMetadataFolder
        ]
        remoteKeynoteIndexZip.parent = remoteKeynoteBundle
        remoteKeynoteRandomFile.parent = remoteKeynoteBundle
        remoteKeynoteDataFolder.parent = remoteKeynoteBundle
        remoteKeynoteDataRandomFile.parent = remoteKeynoteDataFolder
        remoteKeynoteDataFolder.children = [remoteKeynoteDataRandomFile]
        remoteKeynoteMetadataFolder.parent = remoteKeynoteBundle
        remoteKeynoteMetadataFolder.children = [
            remoteKeynoteDocIdentifier,
            remoteKeynoteVersionPlist,
            remoteKeynotePropertiesPlist
        ]
        remoteKeynoteDocIdentifier.parent = remoteKeynoteMetadataFolder
        remoteKeynoteVersionPlist.parent = remoteKeynoteMetadataFolder
        remoteKeynotePropertiesPlist.parent = remoteKeynoteMetadataFolder

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let bundleItemMetadata = remoteKeynoteBundle.toItemMetadata(account: Self.account)
        bundleItemMetadata.contentType = UTType.bundle.identifier
        Self.dbManager.addItemMetadata(bundleItemMetadata)

        let bundleIndexZipMetadata = remoteKeynoteIndexZip.toItemMetadata(account: Self.account)
        bundleIndexZipMetadata.classFile = NKCommon.TypeClassFile.compress.rawValue
        bundleIndexZipMetadata.contentType = UTType.zip.identifier
        Self.dbManager.addItemMetadata(bundleIndexZipMetadata)

        let bundleRandomFileMetadata = remoteKeynoteRandomFile.toItemMetadata(account: Self.account)
        bundleRandomFileMetadata.contentType = UTType.text.identifier
        Self.dbManager.addItemMetadata(bundleRandomFileMetadata)

        let bundleDataFolderMetadata = remoteKeynoteDataFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(bundleDataFolderMetadata)

        let bundleDataRandomFileMetadata =
            remoteKeynoteDataRandomFile.toItemMetadata(account: Self.account)
        bundleDataRandomFileMetadata.classFile = NKCommon.TypeClassFile.image.rawValue
        bundleDataRandomFileMetadata.contentType = UTType.image.identifier
        Self.dbManager.addItemMetadata(bundleDataRandomFileMetadata)

        let bundleMetadataFolderMetadata = remoteKeynoteMetadataFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(bundleMetadataFolderMetadata)

        let bundleDocIdentifierMetadata =
            remoteKeynoteDocIdentifier.toItemMetadata(account: Self.account)
        bundleDocIdentifierMetadata.contentType = UTType.text.identifier
        Self.dbManager.addItemMetadata(bundleDocIdentifierMetadata)

        let bundleVersionPlistMetadata =
            remoteKeynoteVersionPlist.toItemMetadata(account: Self.account)
        bundleVersionPlistMetadata.contentType = UTType.xml.identifier
        Self.dbManager.addItemMetadata(bundleVersionPlistMetadata)

        let bundlePropertiesPlistMetadata =
            remoteKeynotePropertiesPlist.toItemMetadata(account: Self.account)
        bundlePropertiesPlistMetadata.size = Int64(remoteKeynotePropertiesPlist.data?.count ?? 0)
        bundlePropertiesPlistMetadata.directory = false
        bundlePropertiesPlistMetadata.contentType = UTType.xml.identifier

        Self.dbManager.addItemMetadata(bundlePropertiesPlistMetadata)

        let bundleItem = Item(
            metadata: bundleItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )
        bundleItem.dbManager = Self.dbManager

        let fm = FileManager.default
        let tempUrl = fm.temporaryDirectory.appendingPathComponent(keynoteBundleFilename)
        try fm.createDirectory(at: tempUrl, withIntermediateDirectories: true, attributes: nil)
        let keynoteIndexZipPath = tempUrl.appendingPathComponent("Index.zip")
        try Data("This is a fake zip (but new!)".utf8).write(to: keynoteIndexZipPath)
        let keynoteDataDir = tempUrl.appendingPathComponent("Data")
        try fm.createDirectory(
            at: keynoteDataDir, withIntermediateDirectories: true, attributes: nil
        )
        let keynoteMetadataDir = tempUrl.appendingPathComponent("Metadata")
        try fm.createDirectory(
            at: keynoteMetadataDir, withIntermediateDirectories: true, attributes: nil
        )
        let keynoteDocIdentifierPath =
            keynoteMetadataDir.appendingPathComponent("DocumentIdentifier")
        try Data("82LQN84b-12JF-BV90-13F0-149UFRN241B".utf8).write(to: keynoteDocIdentifierPath)
        let keynoteBuildVersionPlistPath =
            keynoteMetadataDir.appendingPathComponent("BuildVersionHistory.plist")
        try Data(
"""
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <string>Template: 34_DynamicWaves (15.0)</string>
    <string>M15.0-7040.0.73-4</string>
</array>
</plist>
"""
            .utf8).write(to: keynoteBuildVersionPlistPath)
        let keynotePropertiesPlistPath =
            keynoteMetadataDir.appendingPathComponent("Properties.plist")
        try Data(
"""
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>revision</key>
    <string>SOME-RANDOM-REVISION-STRING</string>
    <key>documentUUID</key>
    <string>82LQN84b-12JF-BV90-13F0-149UFRN241B</string>
    <key>versionUUID</key>
    <string>VERSION-BEEP-BOOP-HEHE</string>
    <key>privateUUID</key>
    <string>PRIVATE-UUID-BEEP-BOOP-HEHE</string>
    <key>isMultiPage</key>
    <false/>
    <key>stableDocumentUUID</key>
    <string>82LQN84b-12JF-BV90-13F0-149UFRN241B</string>
    <key>fileFormatVersion</key>
    <string>15.0</string>
    <key>shareUUID</key>
    <string>82LQN84b-12JF-BV90-13F0-149UFRN241B</string>
</dict>
</plist>
"""
            .utf8).write(to: keynotePropertiesPlistPath)

        let targetBundleMetadata = ItemMetadata()
        targetBundleMetadata.ocId = remoteKeynoteBundle.identifier
        targetBundleMetadata.etag = "this-is-a-new-etag"
        targetBundleMetadata.name = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileName = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileNameView = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move
        targetBundleMetadata.urlBase = Self.account.serverUrl
        targetBundleMetadata.account = Self.account.ncKitAccount
        targetBundleMetadata.userId = Self.account.id
        targetBundleMetadata.user = Self.account.username
        targetBundleMetadata.date = .init()
        targetBundleMetadata.directory = true
        targetBundleMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        targetBundleMetadata.contentType = UTType.bundle.identifier

        let targetItem = Item(
            metadata: targetBundleMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface
        )
        targetItem.dbManager = Self.dbManager

        let (modifiedItemMaybe, error) = await bundleItem.modify(
            itemTarget: targetItem,
            changedFields: [.filename, .contents, .parentItemIdentifier, .contentModificationDate],
            contents: tempUrl,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedItem = try XCTUnwrap(modifiedItemMaybe)

        XCTAssertEqual(modifiedItem.itemIdentifier, targetItem.itemIdentifier)
        XCTAssertEqual(modifiedItem.filename, targetItem.filename)
        XCTAssertEqual(modifiedItem.parentItemIdentifier, targetItem.parentItemIdentifier)
        // TODO: This is a folder, unfortunately through NCKit we cannot set these details on a
        // TODO: folder's creation; we should fix this
        // XCTAssertEqual(modifiedItem.contentModificationDate, targetItem.contentModificationDate)

        XCTAssertEqual(remoteFolder.children.count, 1)
        XCTAssertEqual(remoteFolder.children.first, remoteKeynoteBundle)
        XCTAssertEqual(remoteKeynoteBundle.name, targetBundleMetadata.fileName)
        XCTAssertEqual(
            remoteKeynoteBundle.remotePath,
            targetBundleMetadata.serverUrl + "/" + targetBundleMetadata.fileName
        )

        XCTAssertNil(remoteKeynoteBundle.children.first { $0.name == keynoteRandomFileName })
        XCTAssertNil(
            remoteKeynoteDataFolder.children.first { $0.name == keynoteDataRandomImageName }
        )

        XCTAssertEqual(remoteKeynoteBundle.children.count, 3)
        XCTAssertNotNil(remoteKeynoteBundle.children.first { $0.name == keynoteIndexZipFilename })
        XCTAssertNotNil(remoteKeynoteBundle.children.first { $0.name == keynoteMetadataFolderName })
        XCTAssertNotNil(remoteKeynoteBundle.children.first { $0.name == keynoteDataFolderName })
        XCTAssertEqual(remoteKeynoteDataFolder.children.count, 0)
        XCTAssertEqual(remoteKeynoteMetadataFolder.children.count, 3)
        XCTAssertNotNil(
            remoteKeynoteMetadataFolder.children.first { $0.name == keynoteDocIdentifierFilename }
        )
        XCTAssertNotNil(
            remoteKeynoteMetadataFolder.children.first { $0.name == keynoteVersionPlistFilename }
        )
        XCTAssertNotNil(
            remoteKeynoteMetadataFolder.children.first { $0.name == keynotePropertiesPlistFilename }
        )

        XCTAssertEqual(remoteKeynoteIndexZip.data, try Data(contentsOf: keynoteIndexZipPath))
        XCTAssertEqual(
            remoteKeynoteDocIdentifier.data, try Data(contentsOf: keynoteDocIdentifierPath)
        )
        XCTAssertEqual(
            remoteKeynoteVersionPlist.data, try Data(contentsOf: keynoteBuildVersionPlistPath)
        )
        XCTAssertEqual(
            remoteKeynotePropertiesPlist.data, try Data(contentsOf: keynotePropertiesPlistPath)
        )
    }

    func testMoveFileToTrash() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )
        let trashItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        item.dbManager = Self.dbManager

        let (trashedItemMaybe, error) = await item.modify(
            itemTarget: trashItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 1)
        let remoteTrashedItem = rootTrashItem.children.first
        XCTAssertNotNil(remoteTrashedItem)

        let trashedItem = try XCTUnwrap(trashedItemMaybe)
        XCTAssertEqual(
            trashedItem.itemIdentifier.rawValue, remoteTrashedItem?.identifier
        )
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedItem.filename, remoteTrashedItem?.name)
        XCTAssertEqual(trashedItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedItem.metadata.trashbinOriginalLocation,
            (itemMetadata.serverUrl + "/" + itemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl, with: "")
        )
        XCTAssertEqual(trashedItem.parentItemIdentifier, .trashContainer)
    }

    func testMoveFolderToTrash() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: remoteFolder.remotePath + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = remoteFolder.versionIdentifier
        folderMetadata.directory = remoteFolder.directory
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.urlBase = Self.account.serverUrl
        folderMetadata.userId = Self.account.id
        folderMetadata.user = Self.account.username
        folderMetadata.account = Self.account.ncKitAccount

        Self.dbManager.addItemMetadata(folderMetadata)

        let itemMetadata = ItemMetadata()
        itemMetadata.ocId = remoteItem.identifier
        itemMetadata.etag = remoteItem.versionIdentifier
        itemMetadata.name = remoteItem.name
        itemMetadata.fileName = remoteItem.name
        itemMetadata.fileNameView = remoteItem.name
        itemMetadata.serverUrl = folderMetadata.serverUrl + "/" + folderMetadata.fileName
        itemMetadata.urlBase = Self.account.serverUrl
        itemMetadata.userId = Self.account.id
        itemMetadata.user = Self.account.username
        itemMetadata.account = Self.account.ncKitAccount

        Self.dbManager.addItemMetadata(itemMetadata)

        let folderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )
        let trashFolderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        folderItem.dbManager = Self.dbManager

        let (trashedFolderItemMaybe, error) = await folderItem.modify(
            itemTarget: trashFolderItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 1)
        let remoteTrashedFolderItem = rootTrashItem.children.first
        XCTAssertNotNil(remoteTrashedFolderItem)

        let trashedFolderItem = try XCTUnwrap(trashedFolderItemMaybe)
        XCTAssertEqual(
            trashedFolderItem.itemIdentifier.rawValue, remoteTrashedFolderItem?.identifier
        )
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedFolderItem.filename, remoteTrashedFolderItem?.name)
        XCTAssertEqual(trashedFolderItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedFolderItem.metadata.trashbinOriginalLocation,
            (folderMetadata.serverUrl + "/" + folderMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl, with: "")
        )
        XCTAssertEqual(trashedFolderItem.parentItemIdentifier, .trashContainer)

        let trashChildItemMetadata = Self.dbManager.itemMetadataFromOcId(itemMetadata.ocId)
        XCTAssertNotNil(trashChildItemMetadata)
        XCTAssertEqual(trashChildItemMetadata?.isTrashed, true)
        XCTAssertEqual(
            trashChildItemMetadata?.serverUrl,
            trashedFolderItem.metadata.serverUrl + "/" + trashedFolderItem.filename
        )
        XCTAssertEqual(trashChildItemMetadata?.trashbinFileName, itemMetadata.fileName)
        XCTAssertEqual(
            trashChildItemMetadata?.trashbinOriginalLocation,
            (itemMetadata.serverUrl + "/" + itemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
    }

    func testMoveFolderToTrashWithRename() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: remoteFolder.remotePath + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = remoteFolder.versionIdentifier
        folderMetadata.directory = remoteFolder.directory
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.urlBase = Self.account.serverUrl
        folderMetadata.userId = Self.account.id
        folderMetadata.user = Self.account.username
        folderMetadata.account = Self.account.ncKitAccount

        Self.dbManager.addItemMetadata(folderMetadata)

        let renamedFolderMetadata = ItemMetadata(value: folderMetadata)
        renamedFolderMetadata.fileName = "folder (renamed)"

        let itemMetadata = ItemMetadata()
        itemMetadata.ocId = remoteItem.identifier
        itemMetadata.etag = remoteItem.versionIdentifier
        itemMetadata.name = remoteItem.name
        itemMetadata.fileName = remoteItem.name
        itemMetadata.fileNameView = remoteItem.name
        itemMetadata.serverUrl = folderMetadata.serverUrl + "/" + folderMetadata.fileName
        itemMetadata.urlBase = Self.account.serverUrl
        itemMetadata.userId = Self.account.id
        itemMetadata.user = Self.account.username
        itemMetadata.account = Self.account.ncKitAccount

        Self.dbManager.addItemMetadata(itemMetadata)

        let folderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )
        let trashFolderItem = Item(
            metadata: renamedFolderMetadata, // Test rename first and then trash
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface
        )

        folderItem.dbManager = Self.dbManager

        let (trashedFolderItemMaybe, error) = await folderItem.modify(
            itemTarget: trashFolderItem,
            changedFields: [.parentItemIdentifier, .filename],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 1)
        let remoteTrashedFolderItem = rootTrashItem.children.first
        XCTAssertNotNil(remoteTrashedFolderItem)

        let trashedFolderItem = try XCTUnwrap(trashedFolderItemMaybe)
        XCTAssertEqual(
            trashedFolderItem.itemIdentifier.rawValue, remoteTrashedFolderItem?.identifier
        )
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedFolderItem.filename, remoteTrashedFolderItem?.name)
        XCTAssertEqual(trashedFolderItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedFolderItem.metadata.trashbinOriginalLocation,
            (renamedFolderMetadata.serverUrl + "/" + renamedFolderMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl, with: "")
        )
        XCTAssertEqual(trashedFolderItem.parentItemIdentifier, .trashContainer)

        let trashChildItemMetadata = Self.dbManager.itemMetadataFromOcId(itemMetadata.ocId)
        XCTAssertNotNil(trashChildItemMetadata)
        XCTAssertEqual(trashChildItemMetadata?.isTrashed, true)
        XCTAssertEqual(
            trashChildItemMetadata?.serverUrl,
            trashedFolderItem.metadata.serverUrl + "/" + trashedFolderItem.filename
        )
        XCTAssertEqual(trashChildItemMetadata?.trashbinFileName, itemMetadata.fileName)
        XCTAssertEqual(
            trashChildItemMetadata?.trashbinOriginalLocation,
            (renamedFolderMetadata.serverUrl + "/" + renamedFolderMetadata.fileName + "/" + itemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
    }
}
