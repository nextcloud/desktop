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

    func testModifyFileContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: Self.account.davFilesUrl + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteItem, remoteFolder]
        remoteItem.parent = rootItem
        remoteFolder.parent = rootItem

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = remoteFolder.versionIdentifier
        folderMetadata.directory = remoteFolder.directory
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.urlBase = Self.account.serverUrl
        folderMetadata.userId = Self.account.username
        folderMetadata.user = Self.account.username

        Self.dbManager.addItemMetadata(folderMetadata)

        let itemMetadata = ItemMetadata()
        itemMetadata.ocId = remoteItem.identifier
        itemMetadata.etag = remoteItem.versionIdentifier
        itemMetadata.name = remoteItem.name
        itemMetadata.fileName = remoteItem.name
        itemMetadata.fileNameView = remoteItem.name
        itemMetadata.serverUrl = Self.account.davFilesUrl
        itemMetadata.urlBase = Self.account.serverUrl
        itemMetadata.userId = Self.account.username
        itemMetadata.user = Self.account.username

        Self.dbManager.addItemMetadata(itemMetadata)

        let targetItemMetadata = ItemMetadata()
        targetItemMetadata.ocId = remoteItem.identifier
        targetItemMetadata.etag = remoteItem.identifier
        targetItemMetadata.name = "item-renamed.txt" // Renamed
        targetItemMetadata.fileName = "item-renamed.txt" // Renamed
        targetItemMetadata.fileNameView = "item-renamed.txt" // Renamed
        targetItemMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move
        targetItemMetadata.urlBase = Self.account.serverUrl
        targetItemMetadata.userId = Self.account.username
        targetItemMetadata.user = Self.account.username
        targetItemMetadata.date = .init()

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )

        let targetItem = Item(
            metadata: targetItemMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
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
            ncAccount: Self.account,
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
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let keynoteBundleFilename = "test.key"
        let remoteFolder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteBundle = MockRemoteItem(
            identifier: keynoteBundleFilename,
            name: keynoteBundleFilename,
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteDataFolder = MockRemoteItem(
            identifier: keynoteBundleFilename + "/Data",
            name: "Data",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/Data",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteMetadataFolder = MockRemoteItem(
            identifier: keynoteBundleFilename + "/Metadata",
            name: "Metadata",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/Metadata",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteIndexZip = MockRemoteItem(
            identifier: keynoteBundleFilename + "/Index.zip",
            name: "Index.zip",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/Index.zip",
            data: "This is a fake zip, pre modification".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteRandomFile = MockRemoteItem( // We will want this to be gone later
            identifier: keynoteBundleFilename + "/random.txt",
            name: "random.txt",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/random.txt",
            data: "This is a random file, I should be gone post modify".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteDocIdentifier = MockRemoteItem(
            identifier: keynoteBundleFilename + "/Metadata/DocumentIdentifier",
            name: "DocumentIdentifier",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/Metadata/DocumentIdentifier",
            data: "8B0C6C1F-4DA4-4DE8-8510-0C91FDCE7D01".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynoteVersionPlist = MockRemoteItem(
            identifier: keynoteBundleFilename + "/Metadata/BuildVersionHistory.plist",
            name: "BuildVersionHistory.plist",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/Metadata/BuildVersionHistory.plist",
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
            serverUrl: Self.account.serverUrl
        )
        let remoteKeynotePropertiesPlist = MockRemoteItem(
            identifier: keynoteBundleFilename + "/Metadata/Properties.plist",
            name: "Properties.plist",
            remotePath: Self.account.davFilesUrl + "/" + keynoteBundleFilename + "/Metadata/Properties.plist",
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
            serverUrl: Self.account.serverUrl
        )

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
        remoteKeynoteMetadataFolder.parent = remoteKeynoteBundle
        remoteKeynoteMetadataFolder.children = [
            remoteKeynoteDocIdentifier,
            remoteKeynoteVersionPlist,
            remoteKeynotePropertiesPlist
        ]
        remoteKeynoteDocIdentifier.parent = remoteKeynoteMetadataFolder
        remoteKeynoteVersionPlist.parent = remoteKeynoteMetadataFolder
        remoteKeynotePropertiesPlist.parent = remoteKeynoteMetadataFolder

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = remoteFolder.versionIdentifier
        folderMetadata.directory = remoteFolder.directory
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.urlBase = Self.account.serverUrl
        folderMetadata.userId = Self.account.username
        folderMetadata.user = Self.account.username

        Self.dbManager.addItemMetadata(folderMetadata)

        let bundleItemMetadata = ItemMetadata()
        bundleItemMetadata.ocId = keynoteBundleFilename
        bundleItemMetadata.etag = "oldat "
        bundleItemMetadata.name = keynoteBundleFilename
        bundleItemMetadata.fileName = keynoteBundleFilename
        bundleItemMetadata.fileNameView = keynoteBundleFilename
        bundleItemMetadata.directory = true
        bundleItemMetadata.serverUrl = Self.account.davFilesUrl
        bundleItemMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        bundleItemMetadata.contentType = UTType.bundle.identifier

        Self.dbManager.addItemMetadata(bundleItemMetadata)

        let bundleItem = Item(
            metadata: bundleItemMetadata,
            parentItemIdentifier: .rootContainer,
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
        targetBundleMetadata.ocId = keynoteBundleFilename
        targetBundleMetadata.etag = "this-is-a-new-etag"
        targetBundleMetadata.name = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileName = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileNameView = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move
        targetBundleMetadata.urlBase = Self.account.serverUrl
        targetBundleMetadata.userId = Self.account.username
        targetBundleMetadata.user = Self.account.username
        targetBundleMetadata.date = .init()
        targetBundleMetadata.directory = true
        targetBundleMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        targetBundleMetadata.contentType = UTType.bundle.identifier

        let targetItem = Item(
            metadata: targetBundleMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            remoteInterface: remoteInterface
        )
        targetItem.dbManager = Self.dbManager

        let (modifiedItemMaybe, error) = await bundleItem.modify(
            itemTarget: targetItem,
            changedFields: [.filename, .contents, .parentItemIdentifier, .contentModificationDate],
            contents: tempUrl,
            ncAccount: Self.account,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedItem = try XCTUnwrap(modifiedItemMaybe)

        // TODO: Unfortunately when deleting the bundle we lose the original identifier.
        // TODO: In the future we should keep the bundle root folder and replace the contents.
        // XCTAssertEqual(modifiedItem.itemIdentifier, targetItem.itemIdentifier)
        XCTAssertEqual(modifiedItem.filename, targetItem.filename)
        XCTAssertEqual(modifiedItem.parentItemIdentifier, targetItem.parentItemIdentifier)
        // TODO: This is a folder, unfortunately through NCKit we cannot set these details on a
        // TODO: folder's creation; we should fix this
        // XCTAssertEqual(modifiedItem.contentModificationDate, targetItem.contentModificationDate)

        XCTAssertFalse(remoteFolder.children.isEmpty)
        XCTAssertEqual(remoteKeynoteBundle.name, targetBundleMetadata.fileName)
        XCTAssertEqual(
            remoteKeynoteBundle.remotePath,
            targetBundleMetadata.serverUrl + "/" + targetBundleMetadata.fileName
        )

        // Get the new remote nodes, since we delete and recreate remotely instead of modifying
        let newRemoteKeynoteBundle = try XCTUnwrap(
            remoteFolder.children.first { $0.name == targetBundleMetadata.fileName }
        )
        XCTAssertNil(newRemoteKeynoteBundle.children.first { $0.name == "random.txt" })
        let newRemoteKeynoteIndexZip = try XCTUnwrap(
            newRemoteKeynoteBundle.children.first { $0.name == "Index.zip" }
        )
        let newRemoteKeynoteDataFolder = try XCTUnwrap(
            newRemoteKeynoteBundle.children.first { $0.name == "Data" }
        )
        let newRemoteKeynoteMetadata = try XCTUnwrap(
            newRemoteKeynoteBundle.children.first { $0.name == "Metadata" }
        )
        let newRemoteKeynoteDocIdentifier = try XCTUnwrap(
            newRemoteKeynoteMetadata.children.first { $0.name == "DocumentIdentifier" }
        )
        let newRemoteKeynoteVersionPlist = try XCTUnwrap(
            newRemoteKeynoteMetadata.children.first { $0.name == "BuildVersionHistory.plist" }
        )
        let newRemoteKeynotePropertiesPlist = try XCTUnwrap(
            newRemoteKeynoteMetadata.children.first { $0.name == "Properties.plist" }
        )

        XCTAssertEqual(newRemoteKeynoteIndexZip.data, try Data(contentsOf: keynoteIndexZipPath))
        XCTAssertEqual(
            newRemoteKeynoteDocIdentifier.data, try Data(contentsOf: keynoteDocIdentifierPath)
        )
        XCTAssertEqual(
            newRemoteKeynoteVersionPlist.data, try Data(contentsOf: keynoteBuildVersionPlistPath)
        )
        XCTAssertEqual(
            newRemoteKeynotePropertiesPlist.data, try Data(contentsOf: keynotePropertiesPlistPath)
        )
    }
}
