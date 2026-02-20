//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import UniformTypeIdentifiers
import XCTest

final class ItemModifyTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    lazy var rootTrashItem = MockRemoteItem.rootTrashItem(account: Self.account)

    var remoteFolder: MockRemoteItem!
    var remoteItem: MockRemoteItem!
    var remoteTrashItem: MockRemoteItem!
    var remoteTrashFolder: MockRemoteItem!
    var remoteTrashFolderChildItem: MockRemoteItem!

    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

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
        remoteTrashItem = MockRemoteItem(
            identifier: "trashItem",
            versionIdentifier: "0",
            name: "trashItem.txt (trashed)",
            remotePath: Self.account.trashUrl + "/trashItem.txt (trashed)",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "folder/trashItem.txt"
        )
        remoteTrashFolder = MockRemoteItem(
            identifier: "trashedFolder",
            versionIdentifier: "0",
            name: "trashedFolder (trashed)",
            remotePath: Self.account.trashUrl + "/trashedFolder (trashed)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "trashedFolder"
        )
        remoteTrashFolderChildItem = MockRemoteItem(
            identifier: "trashChildItem",
            versionIdentifier: "0",
            name: "trashChildItem.txt",
            remotePath: remoteTrashFolder.remotePath + "/trashChildItem.txt",
            data: "Hello world, I'm trash!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "trashedFolder/trashChildItem.txt"
        )

        rootItem.children = [remoteItem, remoteFolder]
        rootTrashItem.children = [remoteTrashItem, remoteTrashFolder]
        remoteItem.parent = rootItem
        remoteFolder.parent = rootItem
        remoteTrashFolder.children = [remoteTrashFolderChildItem]
        remoteTrashFolderChildItem.parent = remoteTrashFolder
    }

    func testModifyFile() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContents = "Hello, New World!".data(using: .utf8)
        let newContentsUrl = FileManager.default.temporaryDirectory.appendingPathComponent("test")
        try newContents?.write(to: newContentsUrl)

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
        targetItemMetadata.name = "item-renamed.txt" // Renamed
        targetItemMetadata.fileName = "item-renamed.txt" // Renamed
        targetItemMetadata.fileNameView = "item-renamed.txt" // Renamed
        targetItemMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move
        targetItemMetadata.date = .init()
        targetItemMetadata.size = try Int64(XCTUnwrap(newContents?.count))

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let targetItem = Item(
            metadata: targetItemMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

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
        XCTAssertEqual(modifiedItem.documentSize?.intValue, newContents?.count)

        XCTAssertFalse(remoteFolder.children.isEmpty)
        XCTAssertEqual(remoteItem.data, newContents)
        XCTAssertEqual(remoteItem.name, targetItemMetadata.fileName)
        XCTAssertEqual(
            remoteItem.remotePath, targetItemMetadata.serverUrl + "/" + targetItemMetadata.fileName
        )
    }

    func testModifyFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let remoteFolderB = MockRemoteItem(
            identifier: "folder-b",
            name: "folder-b",
            remotePath: Self.account.davFilesUrl + "/folder-b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children = [remoteFolder, remoteFolderB]
        remoteFolder.parent = rootItem
        remoteFolderB.parent = rootItem

        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let folderBMetadata = remoteFolderB.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderBMetadata)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let testingUrl = FileManager.default.temporaryDirectory.appendingPathComponent("nctest-dir")
        do {
            try FileManager.default.createDirectory(
                atPath: testingUrl.path, withIntermediateDirectories: true, attributes: nil
            )
        } catch {
            print(error.localizedDescription)
        }

        var modifiedFolderMetadata = SendableItemMetadata(value: folderMetadata)
        modifiedFolderMetadata.apply(fileName: "folder-renamed")
        modifiedFolderMetadata.serverUrl = remoteFolderB.remotePath

        let folderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let targetFolderItem = Item(
            metadata: modifiedFolderMetadata,
            parentItemIdentifier: .init(remoteFolderB.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedFolderMaybe, error) = await folderItem.modify(
            itemTarget: targetFolderItem,
            changedFields: [.filename, .contents, .parentItemIdentifier, .contentModificationDate],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedFolder = try XCTUnwrap(modifiedFolderMaybe)

        XCTAssertEqual(modifiedFolder.itemIdentifier, targetFolderItem.itemIdentifier)
        XCTAssertEqual(modifiedFolder.filename, targetFolderItem.filename)
        XCTAssertEqual(modifiedFolder.parentItemIdentifier, targetFolderItem.parentItemIdentifier)
        XCTAssertEqual(modifiedFolder.contentModificationDate, targetFolderItem.contentModificationDate)

        XCTAssertEqual(rootItem.children.count, 1)
        XCTAssertEqual(remoteFolder.children.count, 1)
        XCTAssertEqual(remoteFolderB.children.count, 1)
        XCTAssertEqual(remoteFolder.name, targetFolderItem.filename)
        XCTAssertEqual(
            remoteFolder.remotePath, modifiedFolderMetadata.serverUrl + "/" + modifiedFolderMetadata.fileName
        )
        // We do not yet support modification of folder contents
    }

    func testModifyBundleContents() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

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

        var bundleItemMetadata = remoteKeynoteBundle.toItemMetadata(account: Self.account)
        bundleItemMetadata.contentType = UTType.bundle.identifier
        Self.dbManager.addItemMetadata(bundleItemMetadata)

        var bundleIndexZipMetadata = remoteKeynoteIndexZip.toItemMetadata(account: Self.account)
        bundleIndexZipMetadata.classFile = NKTypeClassFile.compress.rawValue
        bundleIndexZipMetadata.contentType = UTType.zip.identifier
        Self.dbManager.addItemMetadata(bundleIndexZipMetadata)

        var bundleRandomFileMetadata = remoteKeynoteRandomFile.toItemMetadata(account: Self.account)
        bundleRandomFileMetadata.contentType = UTType.text.identifier
        Self.dbManager.addItemMetadata(bundleRandomFileMetadata)

        let bundleDataFolderMetadata = remoteKeynoteDataFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(bundleDataFolderMetadata)

        var bundleDataRandomFileMetadata =
            remoteKeynoteDataRandomFile.toItemMetadata(account: Self.account)
        bundleDataRandomFileMetadata.classFile = NKTypeClassFile.image.rawValue
        bundleDataRandomFileMetadata.contentType = UTType.image.identifier
        Self.dbManager.addItemMetadata(bundleDataRandomFileMetadata)

        let bundleMetadataFolderMetadata = remoteKeynoteMetadataFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(bundleMetadataFolderMetadata)

        var bundleDocIdentifierMetadata =
            remoteKeynoteDocIdentifier.toItemMetadata(account: Self.account)
        bundleDocIdentifierMetadata.contentType = UTType.text.identifier
        Self.dbManager.addItemMetadata(bundleDocIdentifierMetadata)

        var bundleVersionPlistMetadata =
            remoteKeynoteVersionPlist.toItemMetadata(account: Self.account)
        bundleVersionPlistMetadata.contentType = UTType.xml.identifier
        Self.dbManager.addItemMetadata(bundleVersionPlistMetadata)

        var bundlePropertiesPlistMetadata =
            remoteKeynotePropertiesPlist.toItemMetadata(account: Self.account)
        bundlePropertiesPlistMetadata.size = Int64(remoteKeynotePropertiesPlist.data?.count ?? 0)
        bundlePropertiesPlistMetadata.directory = false
        bundlePropertiesPlistMetadata.contentType = UTType.xml.identifier

        Self.dbManager.addItemMetadata(bundlePropertiesPlistMetadata)

        let bundleItem = Item(
            metadata: bundleItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

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
            .utf8
        ).write(to: keynoteBuildVersionPlistPath)
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
            .utf8
        ).write(to: keynotePropertiesPlistPath)

        var targetBundleMetadata = remoteKeynoteBundle.toItemMetadata(account: Self.account)
        targetBundleMetadata.etag = "this-is-a-new-etag"
        targetBundleMetadata.name = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileName = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.fileNameView = "renamed-" + keynoteBundleFilename
        targetBundleMetadata.serverUrl = Self.account.davFilesUrl + "/folder" // Move

        let targetItem = Item(
            metadata: targetBundleMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

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
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let trashItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (trashedItemMaybe, error) = await item.modify(
            itemTarget: trashItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 3)
        let remoteTrashedItem =
            rootTrashItem.children.first(where: { $0.identifier == itemMetadata.ocId + trashedItemIdSuffix })
        XCTAssertNotNil(remoteTrashedItem)

        let trashedItem = try XCTUnwrap(trashedItemMaybe)
        XCTAssertEqual(
            trashedItem.itemIdentifier.rawValue + trashedItemIdSuffix, remoteTrashedItem?.identifier
        )
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedItem.metadata.fileName, remoteTrashedItem?.name)
        XCTAssertEqual(trashedItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedItem.metadata.trashbinOriginalLocation,
            (itemMetadata.serverUrl + "/" + itemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
        XCTAssertEqual(trashedItem.parentItemIdentifier, .trashContainer)
    }

    func testRenameMoveFileToTrash() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let (_, _, initMoveError) = await remoteInterface.move(
            remotePathSource: remoteItem.remotePath,
            remotePathDestination: remoteFolder.remotePath + "/" + remoteItem.name,
            account: Self.account
        )
        XCTAssertEqual(initMoveError, .success)

        let folderMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        var renamedItemMetadata = SendableItemMetadata(value: itemMetadata)
        renamedItemMetadata.name = "renamed"
        renamedItemMetadata.fileName = "renamed"
        renamedItemMetadata.fileNameView = "renamed"

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let trashItem = Item(
            metadata: renamedItemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (trashedItemMaybe, error) = await item.modify(
            itemTarget: trashItem,
            changedFields: [.parentItemIdentifier, .filename],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 3)
        let remoteTrashedItem = rootTrashItem.children.first(
            where: { $0.identifier == itemMetadata.ocId + trashedItemIdSuffix }
        )
        XCTAssertNotNil(remoteTrashedItem)

        let trashedItem = try XCTUnwrap(trashedItemMaybe)
        XCTAssertEqual(
            trashedItem.itemIdentifier.rawValue + trashedItemIdSuffix, remoteTrashedItem?.identifier
        )
        XCTAssertTrue(remoteTrashedItem?.name.hasPrefix(renamedItemMetadata.fileName) ?? false)
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedItem.metadata.fileName, remoteTrashedItem?.name)
        XCTAssertEqual(trashedItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedItem.metadata.trashbinOriginalLocation,
            (remoteFolder.remotePath + "/" + renamedItemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
        XCTAssertEqual(trashedItem.parentItemIdentifier, .trashContainer)
    }

    func testMoveFolderToTrash() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
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

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let folderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let trashFolderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (trashedFolderItemMaybe, error) = await folderItem.modify(
            itemTarget: trashFolderItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 3)
        let remoteTrashedFolderItem = rootTrashItem.children.first(
            where: { $0.identifier == folderMetadata.ocId + trashedItemIdSuffix }
        )
        XCTAssertNotNil(remoteTrashedFolderItem)

        let trashedFolderItem = try XCTUnwrap(trashedFolderItemMaybe)
        XCTAssertEqual(
            trashedFolderItem.itemIdentifier.rawValue + trashedItemIdSuffix,
            remoteTrashedFolderItem?.identifier
        )
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedFolderItem.metadata.fileName, remoteTrashedFolderItem?.name)
        XCTAssertEqual(trashedFolderItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedFolderItem.metadata.trashbinOriginalLocation,
            (folderMetadata.serverUrl + "/" + folderMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
        XCTAssertEqual(trashedFolderItem.parentItemIdentifier, .trashContainer)

        let trashChildItemMetadata = Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)
        XCTAssertNotNil(trashChildItemMetadata)
        XCTAssertEqual(trashChildItemMetadata?.isTrashed, true)
        XCTAssertEqual(
            trashChildItemMetadata?.serverUrl,
            trashedFolderItem.metadata.serverUrl + "/" + trashedFolderItem.metadata.fileName
        )
        XCTAssertEqual(trashChildItemMetadata?.trashbinFileName, itemMetadata.fileName)
        XCTAssertEqual(
            trashChildItemMetadata?.trashbinOriginalLocation,
            (itemMetadata.serverUrl + "/" + itemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
    }

    func testMoveFolderToTrashWithRename() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
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

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        var renamedFolderMetadata = SendableItemMetadata(value: folderMetadata)
        renamedFolderMetadata.fileName = "folder (renamed)"

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let folderItem = Item(
            metadata: folderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let trashFolderItem = Item(
            metadata: renamedFolderMetadata, // Test rename first and then trash
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (trashedFolderItemMaybe, error) = await folderItem.modify(
            itemTarget: trashFolderItem,
            changedFields: [.parentItemIdentifier, .filename],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        XCTAssertEqual(rootTrashItem.children.count, 3)
        let remoteTrashedFolderItem = rootTrashItem.children.first(
            where: { $0.identifier == folderMetadata.ocId + trashedItemIdSuffix }
        )
        XCTAssertNotNil(remoteTrashedFolderItem)

        let trashedFolderItem = try XCTUnwrap(trashedFolderItemMaybe)
        XCTAssertEqual(
            trashedFolderItem.itemIdentifier.rawValue + trashedItemIdSuffix,
            remoteTrashedFolderItem?.identifier
        )
        // The mock remote interface renames items when trashing them, so, ensure this is synced
        XCTAssertEqual(trashedFolderItem.metadata.fileName, remoteTrashedFolderItem?.name)
        XCTAssertEqual(trashedFolderItem.metadata.isTrashed, true)
        XCTAssertEqual(
            trashedFolderItem.metadata.trashbinOriginalLocation,
            (renamedFolderMetadata.serverUrl + "/" + renamedFolderMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
        XCTAssertEqual(trashedFolderItem.parentItemIdentifier, .trashContainer)

        let trashChildItemMetadata = Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)
        XCTAssertNotNil(trashChildItemMetadata)
        XCTAssertEqual(trashChildItemMetadata?.isTrashed, true)
        XCTAssertEqual(
            trashChildItemMetadata?.serverUrl,
            trashedFolderItem.metadata.serverUrl + "/" + trashedFolderItem.metadata.fileName
        )
        XCTAssertEqual(trashChildItemMetadata?.trashbinFileName, itemMetadata.fileName)
        XCTAssertEqual(
            trashChildItemMetadata?.trashbinOriginalLocation,
            (renamedFolderMetadata.serverUrl + "/" + renamedFolderMetadata.fileName + "/" + itemMetadata.fileName)
                .replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")
        )
    }

    func testTrashAndMoveFileOutOfTrash() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let trashItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (trashedItemMaybe, trashError) = await item.modify(
            itemTarget: trashItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(trashError)
        let trashedItem = try XCTUnwrap(trashedItemMaybe)
        XCTAssertEqual(trashedItem.parentItemIdentifier, .trashContainer)

        let (untrashedItemMaybe, untrashError) = await trashedItem.modify(
            itemTarget: item,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(untrashError)
        let untrashedItem = try XCTUnwrap(untrashedItemMaybe)
        XCTAssertEqual(untrashedItem.parentItemIdentifier, .rootContainer)
    }

    func testMoveTrashedFileOutOfTrash() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        let trashItemMetadata = remoteTrashItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashItemMetadata)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let trashItem = Item(
            metadata: trashItemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let untrashedTargetItem = Item(
            metadata: trashItemMetadata,
            parentItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (untrashedItemMaybe, untrashError) = await trashItem.modify(
            itemTarget: untrashedTargetItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(untrashError)
        let untrashedItem = try XCTUnwrap(untrashedItemMaybe)
        XCTAssertEqual(untrashedItem.parentItemIdentifier, .init(remoteFolder.identifier))
    }

    func testMoveTrashedFileOutOfTrashAndRenameAndModifyContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        let trashItemMetadata = remoteTrashItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashItemMetadata)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let newContents = "I've changed!".data(using: .utf8)!
        let newContentsUrl = FileManager.default.temporaryDirectory.appendingPathComponent("test")
        try newContents.write(to: newContentsUrl)

        var targetItemMetadata = SendableItemMetadata(value: trashItemMetadata)
        targetItemMetadata.serverUrl = Self.account.davFilesUrl
        targetItemMetadata.fileName = "new-file.txt"
        targetItemMetadata.fileNameView = "new-file.txt"
        targetItemMetadata.name = "new-file.txt"
        targetItemMetadata.size = Int64(newContents.count)
        targetItemMetadata.trashbinFileName = ""
        targetItemMetadata.trashbinOriginalLocation = ""
        targetItemMetadata.trashbinDeletionTime = Date()

        let trashItem = Item(
            metadata: trashItemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let targetItem = Item(
            metadata: targetItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedUntrashedItemMaybe, error) = await trashItem.modify(
            itemTarget: targetItem,
            changedFields: [.parentItemIdentifier, .filename, .contents],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)

        let modifiedUntrashedItem = try XCTUnwrap(modifiedUntrashedItemMaybe)

        XCTAssertEqual(modifiedUntrashedItem.parentItemIdentifier, .rootContainer)
        XCTAssertEqual(modifiedUntrashedItem.itemIdentifier, targetItem.itemIdentifier)
        XCTAssertEqual(modifiedUntrashedItem.filename, targetItem.filename)
        XCTAssertEqual(modifiedUntrashedItem.documentSize?.int64Value, targetItemMetadata.size)

        XCTAssertEqual(remoteTrashItem.name, targetItem.filename)
        XCTAssertEqual(remoteTrashItem.data, newContents)
    }

    func testMoveFileOutOfTrashWithExistingIdenticallyNamedFile() async throws {
        // Make sure that we properly get the post-untrash state of the target item and not the
        // identically-named file in the location the file has been untrashed to
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        remoteTrashItem.trashbinOriginalLocation =
            remoteItem.remotePath.replacingOccurrences(of: Self.account.davFilesUrl + "/", with: "")

        let trashItemMetadata = remoteTrashItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashItemMetadata)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let trashItem = Item(
            metadata: trashItemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let untrashedTargetItem = Item(
            metadata: trashItemMetadata,
            parentItemIdentifier: .init(rootItem.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (untrashedItemMaybe, untrashError) = await trashItem.modify(
            itemTarget: untrashedTargetItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(untrashError)
        let untrashedItem = try XCTUnwrap(untrashedItemMaybe)
        XCTAssertEqual(untrashedItem.itemIdentifier, trashItem.itemIdentifier)
        XCTAssertEqual(untrashedItem.parentItemIdentifier, .init(rootItem.identifier))
    }

    func testMoveFolderOutOfTrash() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let trashFolderMetadata = remoteTrashFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashFolderMetadata)

        let trashFolderChildItemMetadata =
            remoteTrashFolderChildItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashFolderChildItemMetadata)

        let trashedFolderItem = Item(
            metadata: trashFolderMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let untrashedTargetItem = Item(
            metadata: trashFolderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (untrashedFolderItemMaybe, untrashError) = await trashedFolderItem.modify(
            itemTarget: untrashedTargetItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(untrashError)
        let untrashedItem = try XCTUnwrap(untrashedFolderItemMaybe)
        XCTAssertEqual(untrashedItem.parentItemIdentifier, .rootContainer)
        XCTAssertEqual(remoteTrashFolder.children.count, 1)
        XCTAssertTrue(remoteTrashFolder.remotePath.hasPrefix(Self.account.davFilesUrl))

        let untrashedFolderChildItemMaybe =
            Self.dbManager.itemMetadata(ocId: remoteTrashFolderChildItem.identifier)
        let untrashedFolderChildItem = try XCTUnwrap(untrashedFolderChildItemMaybe)
        XCTAssertEqual(remoteTrashFolder.children.first?.identifier, untrashedFolderChildItem.ocId)
        XCTAssertEqual(
            remoteTrashFolderChildItem.remotePath,
            remoteTrashFolder.remotePath + "/" + remoteTrashFolderChildItem.name
        )
        XCTAssertEqual(untrashedFolderChildItem.serverUrl, remoteTrashFolder.remotePath)
    }

    func testMoveFolderOutOfTrashAndRename() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        let trashFolderMetadata = remoteTrashFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashFolderMetadata)

        var renamedTrashFolderMetadata = SendableItemMetadata(value: trashFolderMetadata)
        renamedTrashFolderMetadata.apply(fileName: "renamed-folder")
        renamedTrashFolderMetadata.serverUrl = Self.account.davFilesUrl
        renamedTrashFolderMetadata.trashbinFileName = ""
        renamedTrashFolderMetadata.trashbinOriginalLocation = ""
        renamedTrashFolderMetadata.trashbinDeletionTime = Date()

        let trashFolderChildItemMetadata =
            remoteTrashFolderChildItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(trashFolderChildItemMetadata)

        let trashedFolderItem = Item(
            metadata: trashFolderMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let untrashedTargetItem = Item(
            metadata: renamedTrashFolderMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (untrashedFolderItemMaybe, untrashError) = await trashedFolderItem.modify(
            itemTarget: untrashedTargetItem,
            changedFields: [.parentItemIdentifier, .filename],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNil(untrashError)
        let untrashedFolderItem = try XCTUnwrap(untrashedFolderItemMaybe)
        XCTAssertEqual(untrashedFolderItem.parentItemIdentifier, .rootContainer)
        XCTAssertEqual(untrashedFolderItem.filename, renamedTrashFolderMetadata.fileName)
        XCTAssertEqual(remoteTrashFolder.children.count, 1)
        XCTAssertEqual(remoteTrashFolder.name, renamedTrashFolderMetadata.fileName)
        XCTAssertTrue(remoteTrashFolder.remotePath.hasPrefix(Self.account.davFilesUrl))

        let untrashedFolderChildItemMaybe =
            Self.dbManager.itemMetadata(ocId: remoteTrashFolderChildItem.identifier)
        let untrashedFolderChildItem = try XCTUnwrap(untrashedFolderChildItemMaybe)
        XCTAssertEqual(remoteTrashFolder.children.first?.identifier, untrashedFolderChildItem.ocId)
        XCTAssertEqual(
            remoteTrashFolderChildItem.remotePath,
            remoteTrashFolder.remotePath + "/" + remoteTrashFolderChildItem.name
        )
        XCTAssertEqual(untrashedFolderChildItem.serverUrl, remoteTrashFolder.remotePath)
    }

    func testModifyFileContentsChunked() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let chunkSize = 2
        let newContents = Data(repeating: 1, count: chunkSize * 3)
        let newContentsUrl = FileManager.default.temporaryDirectory.appendingPathComponent("test")
        try newContents.write(to: newContentsUrl)

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
        targetItemMetadata.date = .init()
        targetItemMetadata.size = Int64(newContents.count)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let targetItem = Item(
            metadata: targetItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItemMaybe, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            forcedChunkSize: chunkSize,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedItem = try XCTUnwrap(modifiedItemMaybe)

        XCTAssertEqual(modifiedItem.itemIdentifier, targetItem.itemIdentifier)
        XCTAssertEqual(modifiedItem.contentModificationDate, targetItem.contentModificationDate)
        XCTAssertEqual(modifiedItem.documentSize?.intValue, newContents.count)

        XCTAssertEqual(remoteItem.data, newContents)
    }

    func testModifyFileContentsChunkedResumed() async throws {
        let chunkSize = 2
        let chunkUploadId = UUID().uuidString
        let previousUploadedChunkNum = 1
        let preexistingChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: chunkUploadId
        )

        let db = Self.dbManager.ncDatabase()
        try db.write {
            db.add([
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 1),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: chunkUploadId
                ),
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 2),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: chunkUploadId
                )
            ])
        }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.currentChunks = [chunkUploadId: [preexistingChunk]]

        var itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        itemMetadata.chunkUploadId = chunkUploadId
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContents = Data(repeating: 1, count: chunkSize * 3)
        let newContentsUrl = FileManager.default.temporaryDirectory.appendingPathComponent("test")
        try newContents.write(to: newContentsUrl)

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
        targetItemMetadata.date = .init()
        targetItemMetadata.size = Int64(newContents.count)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let targetItem = Item(
            metadata: targetItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItemMaybe, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            forcedChunkSize: chunkSize,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedItem = try XCTUnwrap(modifiedItemMaybe)

        XCTAssertEqual(modifiedItem.itemIdentifier, targetItem.itemIdentifier)
        XCTAssertEqual(modifiedItem.contentModificationDate, targetItem.contentModificationDate)
        XCTAssertEqual(modifiedItem.documentSize?.intValue, newContents.count)

        XCTAssertEqual(remoteItem.data, newContents)
        XCTAssertEqual(
            remoteInterface.completedChunkTransferSize[chunkUploadId],
            Int64(newContents.count) - preexistingChunk.size
        )

        let dbItem = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))
        XCTAssertNil(dbItem.chunkUploadId)
    }

    func testModifyDoesNotPropagateIgnoredFile() async {
        let ignoredMatcher = IgnoredFilesMatcher(ignoreList: ["*.bak", "/logs/"], log: FileProviderLogMock())
        let metadata = SendableItemMetadata(
            ocId: "ignored-modify-id",
            fileName: "error.bak",
            account: Self.account
        )
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager
        )
        let (resultItem, error) = await item.modify(
            itemTarget: item,
            changedFields: [.contents],
            contents: nil,
            ignoredFiles: ignoredMatcher,
            dbManager: Self.dbManager
        )
        XCTAssertEqual(error as? NSFileProviderError, NSFileProviderError(.excludedFromSync))
        XCTAssertNotNil(resultItem)
        XCTAssertEqual(resultItem?.metadata.fileName, "error.bak")
    }

    func testModifyCreatesFileThatWasPreviouslyIgnoredWithContentsUrlProvided() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let ignoredMatcher = IgnoredFilesMatcher(ignoreList: ["/logs/"], log: FileProviderLogMock())

        let tempFileName = UUID().uuidString
        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent(tempFileName)
        let modifiedData = try XCTUnwrap("Hello world".data(using: .utf8))
        try modifiedData.write(to: tempUrl)

        var metadata = SendableItemMetadata(
            ocId: UUID().uuidString, // We will still be holding the ID given by fileproviderd
            fileName: "error.bak",
            account: Self.account
        )
        // Imitate expected uploaded/downloaded state
        metadata.uploaded = false
        metadata.downloaded = true
        Self.dbManager.addItemMetadata(metadata)

        var modifiedMetadata = metadata
        modifiedMetadata.size = Int64(modifiedData.count)

        let item = Item(
            metadata: modifiedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (resultItem, error) = await item.modify(
            itemTarget: item,
            changedFields: [.contents],
            contents: tempUrl,
            ignoredFiles: ignoredMatcher,
            dbManager: Self.dbManager
        )

        // Then it should not error and should not propagate changes
        XCTAssertNil(error)
        XCTAssertNotNil(resultItem)

        XCTAssertFalse(rootItem.children.isEmpty)
        let remoteItem = try XCTUnwrap(
            rootItem.children.first { $0.identifier == resultItem?.itemIdentifier.rawValue }
        )
        XCTAssertEqual(remoteItem.name, metadata.fileName)
        XCTAssertEqual(remoteItem.data, modifiedData)
    }

    func testModifyLockFileCompletesWithoutSyncing() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Construct lock file metadata
        let lockFileName = ".~lock.test.doc#"
        var lockFileMetadata = SendableItemMetadata(
            ocId: "lock-id", fileName: lockFileName, account: Self.account
        )
        lockFileMetadata.classFile = "lock"

        let lockItem = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        // Simulate new contents, even though this shouldn't matter
        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent(lockFileName)
        let tempData = try XCTUnwrap(Data("updated lock file".utf8))
        try tempData.write(to: tempUrl)

        var newParent = SendableItemMetadata(ocId: "np", fileName: "np", account: Self.account)
        newParent.serverUrl = Self.account.davFilesUrl
        Self.dbManager.addItemMetadata(newParent)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: newParent.ocId))

        var modifiedMetadata = lockFileMetadata
        modifiedMetadata.fileName = ".~lock.newtest.doc#"
        modifiedMetadata.size = Int64(tempData.count)
        modifiedMetadata.date = Date()
        modifiedMetadata.creationDate = Date(timeIntervalSinceNow: -100)
        let modifyTemplateItem = Item(
            metadata: modifiedMetadata,
            parentItemIdentifier: .init(newParent.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItem, error) = await lockItem.modify(
            itemTarget: modifyTemplateItem,
            changedFields: [
                .filename, .contents, .parentItemIdentifier, .creationDate, .contentModificationDate
            ],
            contents: tempUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(error)
        XCTAssertEqual(modifiedItem?.itemIdentifier, lockItem.itemIdentifier)
        XCTAssertEqual(modifiedItem?.filename, modifiedMetadata.fileName)
        XCTAssertEqual(modifiedItem?.documentSize?.intValue, tempData.count)
        XCTAssertEqual(modifiedItem?.parentItemIdentifier.rawValue, newParent.ocId)
        XCTAssertEqual(modifiedItem?.contentModificationDate, modifiedMetadata.date)
        XCTAssertEqual(modifiedItem?.creationDate, modifiedMetadata.creationDate)
    }

    func testModifyLockFileToNonLockFileCompletesWithSync() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Construct lock file metadata
        let lockFileName = ".~lock.test.doc#"
        var lockFileMetadata = SendableItemMetadata(
            ocId: "lock-id", fileName: lockFileName, account: Self.account
        )
        lockFileMetadata.classFile = "lock"

        let lockItem = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        // Simulate new contents, even though this shouldn't matter
        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent(lockFileName)
        let tempData = try XCTUnwrap(Data("updated, no longer a lock file".utf8))
        try tempData.write(to: tempUrl)

        let newParent = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(newParent)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: newParent.ocId))

        var modifiedMetadata = lockFileMetadata
        modifiedMetadata.fileName = "nolongerlock.txt"
        modifiedMetadata.size = Int64(tempData.count)
        modifiedMetadata.date = Date()
        modifiedMetadata.creationDate = Date(timeIntervalSinceNow: -100)
        let modifyTemplateItem = Item(
            metadata: modifiedMetadata,
            parentItemIdentifier: .init(newParent.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItem, error) = await lockItem.modify(
            itemTarget: modifyTemplateItem,
            changedFields: [
                .filename, .contents, .parentItemIdentifier, .creationDate, .contentModificationDate
            ],
            contents: tempUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(error)

        let remoteItem = try XCTUnwrap(
            remoteFolder.children.first(where: { $0.name == modifiedMetadata.fileName })
        )

        // remote will always give new ocId on create
        XCTAssertNotEqual(modifiedItem?.itemIdentifier, lockItem.itemIdentifier)
        XCTAssertNotEqual(modifiedItem?.itemVersion.contentVersion, lockItem.itemVersion.contentVersion)

        XCTAssertEqual(modifiedItem?.itemIdentifier.rawValue, remoteItem.identifier)
        XCTAssertEqual(modifiedItem?.metadata.etag, remoteItem.versionIdentifier)

        XCTAssertEqual(modifiedItem?.filename, modifiedMetadata.fileName)
        XCTAssertEqual(modifiedItem?.documentSize?.intValue, tempData.count)
        XCTAssertEqual(modifiedItem?.parentItemIdentifier.rawValue, newParent.ocId)
        XCTAssertEqual(modifiedItem?.contentModificationDate, modifiedMetadata.date)

        XCTAssertNotEqual(modifiedItem?.metadata.classFile, "lock")
    }

    func testMoveToTrashFailsWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let trashItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (_, error) = await item.modify(
            itemTarget: trashItem,
            changedFields: [.parentItemIdentifier],
            contents: nil,
            dbManager: Self.dbManager
        )
        XCTAssertNotNil(error)
        XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
    }
}
