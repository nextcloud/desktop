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

    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        rootItem.children = []
        rootTrashItem.children = []
    }

    func testModifyFileContents() async throws {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)
        let remoteItem = MockRemoteItem(
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

        let folderMetadata = ItemMetadata()
        folderMetadata.ocId = remoteFolder.identifier
        folderMetadata.etag = remoteFolder.versionIdentifier
        folderMetadata.directory = remoteFolder.directory
        folderMetadata.name = remoteFolder.name
        folderMetadata.fileName = remoteFolder.name
        folderMetadata.fileNameView = remoteFolder.name
        folderMetadata.serverUrl = Self.account.davFilesUrl
        folderMetadata.urlBase = Self.account.serverUrl
        folderMetadata.account = Self.account.ncKitAccount
        folderMetadata.userId = Self.account.username
        folderMetadata.user = Self.account.username
        folderMetadata.date = remoteFolder.creationDate
        folderMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        folderMetadata.directory = true
        folderMetadata.contentType = UTType.folder.identifier

        Self.dbManager.addItemMetadata(folderMetadata)

        let bundleItemMetadata = ItemMetadata()
        bundleItemMetadata.ocId = remoteKeynoteBundle.identifier
        bundleItemMetadata.etag = remoteKeynoteBundle.versionIdentifier
        bundleItemMetadata.name = remoteKeynoteBundle.name
        bundleItemMetadata.fileName = remoteKeynoteBundle.name
        bundleItemMetadata.fileNameView = remoteKeynoteBundle.name
        bundleItemMetadata.serverUrl = Self.account.davFilesUrl
        bundleItemMetadata.urlBase = Self.account.serverUrl
        bundleItemMetadata.account = Self.account.ncKitAccount
        bundleItemMetadata.userId = Self.account.username
        bundleItemMetadata.user = Self.account.username
        bundleItemMetadata.date = remoteKeynoteBundle.creationDate
        bundleItemMetadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        bundleItemMetadata.directory = true
        bundleItemMetadata.contentType = UTType.bundle.identifier

        Self.dbManager.addItemMetadata(bundleItemMetadata)

        let bundleIndexZipMetadata = ItemMetadata()
        bundleIndexZipMetadata.ocId = remoteKeynoteIndexZip.identifier
        bundleIndexZipMetadata.etag = remoteKeynoteIndexZip.versionIdentifier
        bundleIndexZipMetadata.name = remoteKeynoteIndexZip.name
        bundleIndexZipMetadata.fileName = remoteKeynoteIndexZip.name
        bundleIndexZipMetadata.fileNameView = remoteKeynoteIndexZip.name
        bundleIndexZipMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier
        bundleIndexZipMetadata.urlBase = Self.account.serverUrl
        bundleIndexZipMetadata.account = Self.account.ncKitAccount
        bundleIndexZipMetadata.userId = Self.account.username
        bundleIndexZipMetadata.user = Self.account.username
        bundleIndexZipMetadata.date = remoteKeynoteIndexZip.creationDate
        bundleIndexZipMetadata.size = Int64(remoteKeynoteIndexZip.data?.count ?? 0)
        bundleIndexZipMetadata.directory = false
        bundleIndexZipMetadata.contentType = UTType.zip.identifier

        Self.dbManager.addItemMetadata(bundleIndexZipMetadata)

        let bundleRandomFileMetadata = ItemMetadata()
        bundleRandomFileMetadata.ocId = remoteKeynoteRandomFile.identifier
        bundleRandomFileMetadata.etag = remoteKeynoteRandomFile.versionIdentifier
        bundleRandomFileMetadata.name = remoteKeynoteRandomFile.name
        bundleRandomFileMetadata.fileName = remoteKeynoteRandomFile.name
        bundleRandomFileMetadata.fileNameView = remoteKeynoteRandomFile.name
        bundleRandomFileMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier
        bundleRandomFileMetadata.urlBase = Self.account.serverUrl
        bundleRandomFileMetadata.account = Self.account.ncKitAccount
        bundleRandomFileMetadata.userId = Self.account.username
        bundleRandomFileMetadata.user = Self.account.username
        bundleRandomFileMetadata.date = remoteKeynoteRandomFile.creationDate
        bundleRandomFileMetadata.size = Int64(remoteKeynoteRandomFile.data?.count ?? 0)
        bundleRandomFileMetadata.directory = false
        bundleRandomFileMetadata.contentType = UTType.text.identifier

        Self.dbManager.addItemMetadata(bundleRandomFileMetadata)

        let bundleDataFolderMetadata = ItemMetadata()
        bundleDataFolderMetadata.ocId = remoteKeynoteDataFolder.identifier
        bundleDataFolderMetadata.etag = remoteKeynoteDataFolder.versionIdentifier
        bundleDataFolderMetadata.name = remoteKeynoteDataFolder.name
        bundleDataFolderMetadata.fileName = remoteKeynoteDataFolder.name
        bundleDataFolderMetadata.fileNameView = remoteKeynoteDataFolder.name
        bundleDataFolderMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier
        bundleDataFolderMetadata.urlBase = Self.account.serverUrl
        bundleDataFolderMetadata.account = Self.account.ncKitAccount
        bundleDataFolderMetadata.userId = Self.account.username
        bundleDataFolderMetadata.user = Self.account.username
        bundleDataFolderMetadata.date = remoteKeynoteDataFolder.creationDate
        bundleDataFolderMetadata.directory = true
        bundleDataFolderMetadata.contentType = UTType.folder.identifier

        Self.dbManager.addItemMetadata(bundleDataFolderMetadata)

        let bundleDataRandomFileMetadata = ItemMetadata()
        bundleDataRandomFileMetadata.ocId = remoteKeynoteDataRandomFile.identifier
        bundleDataRandomFileMetadata.etag = remoteKeynoteDataRandomFile.versionIdentifier
        bundleDataRandomFileMetadata.name = remoteKeynoteDataRandomFile.name
        bundleDataRandomFileMetadata.fileName = remoteKeynoteDataRandomFile.name
        bundleDataRandomFileMetadata.fileNameView = remoteKeynoteDataRandomFile.name
        bundleDataRandomFileMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier + "/" + keynoteDataFolderName
        bundleDataRandomFileMetadata.urlBase = Self.account.serverUrl
        bundleDataRandomFileMetadata.account = Self.account.ncKitAccount
        bundleDataRandomFileMetadata.userId = Self.account.username
        bundleDataRandomFileMetadata.user = Self.account.username
        bundleDataRandomFileMetadata.date = remoteKeynoteDataRandomFile.creationDate
        bundleDataRandomFileMetadata.size = Int64(remoteKeynoteDataRandomFile.data?.count ?? 0)
        bundleDataRandomFileMetadata.directory = false
        bundleDataRandomFileMetadata.contentType = UTType.image.identifier

        Self.dbManager.addItemMetadata(bundleDataRandomFileMetadata)

        let bundleMetadataFolderMetadata = ItemMetadata()
        bundleMetadataFolderMetadata.ocId = remoteKeynoteMetadataFolder.identifier
        bundleMetadataFolderMetadata.etag = remoteKeynoteMetadataFolder.versionIdentifier
        bundleMetadataFolderMetadata.name = remoteKeynoteMetadataFolder.name
        bundleMetadataFolderMetadata.fileName = remoteKeynoteMetadataFolder.name
        bundleMetadataFolderMetadata.fileNameView = remoteKeynoteMetadataFolder.name
        bundleMetadataFolderMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier
        bundleMetadataFolderMetadata.urlBase = Self.account.serverUrl
        bundleMetadataFolderMetadata.account = Self.account.ncKitAccount
        bundleMetadataFolderMetadata.userId = Self.account.username
        bundleMetadataFolderMetadata.user = Self.account.username
        bundleMetadataFolderMetadata.date = remoteKeynoteMetadataFolder.creationDate
        bundleMetadataFolderMetadata.directory = true
        bundleMetadataFolderMetadata.contentType = UTType.folder.identifier

        Self.dbManager.addItemMetadata(bundleMetadataFolderMetadata)

        let bundleDocIdentifierMetadata = ItemMetadata()
        bundleDocIdentifierMetadata.ocId = remoteKeynoteDocIdentifier.identifier
        bundleDocIdentifierMetadata.etag = remoteKeynoteDocIdentifier.versionIdentifier
        bundleDocIdentifierMetadata.name = remoteKeynoteDocIdentifier.name
        bundleDocIdentifierMetadata.fileName = remoteKeynoteDocIdentifier.name
        bundleDocIdentifierMetadata.fileNameView = remoteKeynoteDocIdentifier.name
        bundleDocIdentifierMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier + "/" + keynoteMetadataFolderName
        bundleDocIdentifierMetadata.urlBase = Self.account.serverUrl
        bundleDocIdentifierMetadata.account = Self.account.ncKitAccount
        bundleDocIdentifierMetadata.userId = Self.account.username
        bundleDocIdentifierMetadata.user = Self.account.username
        bundleDocIdentifierMetadata.date = remoteKeynoteDocIdentifier.creationDate
        bundleDocIdentifierMetadata.size = Int64(remoteKeynoteDocIdentifier.data?.count ?? 0)
        bundleDocIdentifierMetadata.directory = false
        bundleDocIdentifierMetadata.contentType = UTType.text.identifier

        Self.dbManager.addItemMetadata(bundleDocIdentifierMetadata)

        let bundleVersionPlistMetadata = ItemMetadata()
        bundleVersionPlistMetadata.ocId = remoteKeynoteVersionPlist.identifier
        bundleVersionPlistMetadata.etag = remoteKeynoteVersionPlist.versionIdentifier
        bundleVersionPlistMetadata.name = remoteKeynoteVersionPlist.name
        bundleVersionPlistMetadata.fileName = remoteKeynoteVersionPlist.name
        bundleVersionPlistMetadata.fileNameView = remoteKeynoteVersionPlist.name
        bundleVersionPlistMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier + "/" + keynoteMetadataFolderName
        bundleVersionPlistMetadata.urlBase = Self.account.serverUrl
        bundleVersionPlistMetadata.account = Self.account.ncKitAccount
        bundleVersionPlistMetadata.userId = Self.account.username
        bundleVersionPlistMetadata.user = Self.account.username
        bundleVersionPlistMetadata.date = remoteKeynoteVersionPlist.creationDate
        bundleVersionPlistMetadata.size = Int64(remoteKeynoteVersionPlist.data?.count ?? 0)
        bundleVersionPlistMetadata.directory = false
        bundleVersionPlistMetadata.contentType = UTType.xml.identifier

        Self.dbManager.addItemMetadata(bundleVersionPlistMetadata)

        let bundlePropertiesPlistMetadata = ItemMetadata()
        bundlePropertiesPlistMetadata.ocId = remoteKeynotePropertiesPlist.identifier
        bundlePropertiesPlistMetadata.etag = remoteKeynotePropertiesPlist.versionIdentifier
        bundlePropertiesPlistMetadata.name = remoteKeynotePropertiesPlist.name
        bundlePropertiesPlistMetadata.fileName = remoteKeynotePropertiesPlist.name
        bundlePropertiesPlistMetadata.fileNameView = remoteKeynotePropertiesPlist.name
        bundlePropertiesPlistMetadata.serverUrl = Self.account.davFilesUrl + "/" + remoteKeynoteBundle.identifier + "/" + keynoteMetadataFolderName
        bundlePropertiesPlistMetadata.urlBase = Self.account.serverUrl
        bundlePropertiesPlistMetadata.account = Self.account.ncKitAccount
        bundlePropertiesPlistMetadata.userId = Self.account.username
        bundlePropertiesPlistMetadata.user = Self.account.username
        bundlePropertiesPlistMetadata.date = remoteKeynotePropertiesPlist.creationDate
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
        targetBundleMetadata.ocId = keynoteBundleFilename
        targetBundleMetadata.etag = "this-is-a-new-etag"
        targetBundleMetadata.name = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileName = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileNameView = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move
        targetBundleMetadata.urlBase = Self.account.serverUrl
        targetBundleMetadata.account = Self.account.ncKitAccount
        targetBundleMetadata.userId = Self.account.username
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
