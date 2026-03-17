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

final class ItemCreateTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    var rootItem: MockRemoteItem!
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        rootItem = MockRemoteItem.rootItem(account: Self.account)
    }

    override func tearDown() {
        rootItem.children = []
    }

    func testCreateFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        var folderItemMetadata = SendableItemMetadata(
            ocId: "folder-id", fileName: "folder", account: Self.account
        )
        folderItemMetadata.directory = true
        folderItemMetadata.classFile = NKTypeClassFile.directory.rawValue
        folderItemMetadata.serverUrl = Self.account.davFilesUrl

        let folderItemTemplate = Item(
            metadata: folderItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItemMaybe, error) = await Item.create(
            basedOn: folderItemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
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
            Self.dbManager.itemMetadata(ocId: createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, folderItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, folderItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, folderItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, folderItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdItem.itemIdentifier.rawValue)
        XCTAssertTrue(dbItem.downloaded)
        XCTAssertTrue(dbItem.uploaded)
        XCTAssertTrue(createdItem.isDownloaded)
        XCTAssertTrue(createdItem.isUploaded)
    }

    func testCreateFile() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        try Data("Hello world".utf8).write(to: tempUrl)

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItemMaybe, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
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
            Self.dbManager.itemMetadata(ocId: createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, fileItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, fileItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, fileItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdItem.itemIdentifier.rawValue)
        XCTAssertTrue(dbItem.downloaded)
        XCTAssertTrue(dbItem.uploaded)
        XCTAssertTrue(createdItem.isDownloaded)
        XCTAssertTrue(createdItem.isUploaded)
    }

    func testCreateFileIntoFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var folderItemMetadata = SendableItemMetadata(
            ocId: "folder-id", fileName: "folder", account: Self.account
        )
        folderItemMetadata.directory = true
        folderItemMetadata.classFile = NKTypeClassFile.directory.rawValue
        folderItemMetadata.serverUrl = Self.account.davFilesUrl

        let folderItemTemplate = Item(
            metadata: folderItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (createdFolderItemMaybe, folderError) = await Item.create(
            basedOn: folderItemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNil(folderError)
        let createdFolderItem = try XCTUnwrap(createdFolderItemMaybe)

        let fileRelativeRemotePath = "/folder"
        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue
        fileItemMetadata.serverUrl = Self.account.davFilesUrl + fileRelativeRemotePath

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: createdFolderItem.itemIdentifier,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        try Data("Hello world".utf8).write(to: tempUrl)

        let (createdFileItemMaybe, fileError) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let createdFileItem = try XCTUnwrap(createdFileItemMaybe)

        XCTAssertNil(fileError)
        XCTAssertNotNil(createdFileItem)

        let remoteFolderItem = rootItem.children.first { $0.name == "folder" }
        XCTAssertNotNil(remoteFolderItem)
        XCTAssertFalse(remoteFolderItem?.children.isEmpty ?? true)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdFileItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, fileItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, fileItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, fileItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdFileItem.itemIdentifier.rawValue)
        XCTAssertTrue(dbItem.downloaded)
        XCTAssertTrue(dbItem.uploaded)

        let parentDbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdFolderItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(parentDbItem.fileName, folderItemMetadata.fileName)
        XCTAssertEqual(parentDbItem.fileNameView, folderItemMetadata.fileNameView)
        XCTAssertEqual(parentDbItem.directory, folderItemMetadata.directory)
        XCTAssertEqual(parentDbItem.serverUrl, folderItemMetadata.serverUrl)
        XCTAssertTrue(parentDbItem.downloaded)
        XCTAssertTrue(parentDbItem.uploaded)
    }

    func testCreateBundle() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)

        let keynoteBundleFilename = "test.key"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        var bundleItemMetadata = SendableItemMetadata(
            ocId: "keynotebundleid", fileName: keynoteBundleFilename, account: Self.account
        )
        bundleItemMetadata.directory = true
        bundleItemMetadata.serverUrl = Self.account.davFilesUrl
        bundleItemMetadata.classFile = NKTypeClassFile.directory.rawValue
        bundleItemMetadata.contentType = UTType.bundle.identifier

        let fm = FileManager.default
        let tempUrl = fm.temporaryDirectory.appendingPathComponent(keynoteBundleFilename)
        try fm.createDirectory(at: tempUrl, withIntermediateDirectories: true, attributes: nil)
        let keynoteIndexZipPath = tempUrl.appendingPathComponent("Index.zip")
        try Data("This is a fake zip!".utf8).write(to: keynoteIndexZipPath)
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
        try Data("8B0C6C1F-4DA4-4DE8-8510-0C91FDCE7D01".utf8).write(to: keynoteDocIdentifierPath)
        let keynoteBuildVersionPlistPath =
            keynoteMetadataDir.appendingPathComponent("BuildVersionHistory.plist")
        try Data(
            """
            <?xml version="1.0" encoding="UTF-8"?>
            <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
            <plist version="1.0">
            <array>
                <string>Template: 35_DynamicWavesDark (14.1)</string>
                <string>M14.1-7040.0.73-4</string>
            </array>
            </plist>
            """
            .utf8
        ).write(to: keynoteBuildVersionPlistPath)
        let keynotePropertiesPlistPath = keynoteMetadataDir.appendingPathComponent("Properties.plist")
        try Data(
            """
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
            """
            .utf8
        ).write(to: keynotePropertiesPlistPath)

        let bundleItemTemplate = Item(
            metadata: bundleItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        // TODO: Add fail test with no contents
        let (createdBundleItemMaybe, bundleError) = await Item.create(
            basedOn: bundleItemTemplate,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        let createdBundleItem = try XCTUnwrap(createdBundleItemMaybe)

        XCTAssertNil(bundleError)
        XCTAssertNotNil(createdBundleItem)
        XCTAssertEqual(createdBundleItem.metadata.fileName, bundleItemMetadata.fileName)
        XCTAssertEqual(createdBundleItem.metadata.directory, true)
        XCTAssertTrue(createdBundleItem.isDownloaded)
        XCTAssertTrue(createdBundleItem.isUploaded)

        // Below: this is an upstream issue (which we should fix)
        // XCTAssertTrue(createdBundleItem.contentType.conforms(to: .bundle))

        XCTAssertNotNil(rootItem.children.first { $0.name == bundleItemMetadata.name })
        XCTAssertNotNil(
            rootItem.children.first { $0.identifier == createdBundleItem.itemIdentifier.rawValue }
        )
        let remoteItem = rootItem.children.first { $0.name == bundleItemMetadata.name }
        XCTAssertTrue(remoteItem?.directory ?? false)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdBundleItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, bundleItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, bundleItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, bundleItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, bundleItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdBundleItem.itemIdentifier.rawValue)
        XCTAssertEqual(
            dbItem.etag, String(data: createdBundleItem.itemVersion.contentVersion, encoding: .utf8)
        )
        XCTAssertTrue(dbItem.downloaded)
        XCTAssertTrue(dbItem.uploaded)

        let remoteBundleItem = rootItem.children.first { $0.name == keynoteBundleFilename }
        XCTAssertNotNil(remoteBundleItem)
        XCTAssertEqual(remoteBundleItem?.children.count, 3)

        XCTAssertNotNil(remoteBundleItem?.children.first { $0.name == "Data" })
        XCTAssertNotNil(remoteBundleItem?.children.first { $0.name == "Index.zip" })

        let remoteMetadataItem = remoteBundleItem?.children.first { $0.name == "Metadata" }
        XCTAssertNotNil(remoteMetadataItem)
        XCTAssertEqual(remoteMetadataItem?.children.count, 3)
        XCTAssertNotNil(remoteMetadataItem?.children.first { $0.name == "DocumentIdentifier" })
        XCTAssertNotNil(remoteMetadataItem?.children.first { $0.name == "Properties.plist" })
        XCTAssertNotNil(remoteMetadataItem?.children.first {
            $0.name == "BuildVersionHistory.plist"
        })

        let childrenCount = Self.dbManager.childItemCount(directoryMetadata: dbItem)
        XCTAssertEqual(childrenCount, 6) // Ensure all children recorded to database
    }

    func testCreateFileChunked() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let chunkSize = 2
        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        let tempData = Data(repeating: 1, count: chunkSize * 3)
        try tempData.write(to: tempUrl)

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItemMaybe, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            forcedChunkSize: chunkSize,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
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
        XCTAssertEqual(remoteItem.data, tempData)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, fileItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, fileItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, fileItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdItem.itemIdentifier.rawValue)
        XCTAssertTrue(dbItem.downloaded)
        XCTAssertTrue(dbItem.uploaded)
    }

    func testCreateFileChunkedResumed() async throws {
        let chunkSize = 2
        let expectedChunkUploadId = UUID().uuidString // Check if illegal characters are stripped
        let illegalChunkUploadId = expectedChunkUploadId + "/" // Check if illegal characters are stripped
        let previousUploadedChunkNum = 1
        let preexistingChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: expectedChunkUploadId
        )

        let db = Self.dbManager.ncDatabase()
        try db.write {
            db.add([
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 1),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: expectedChunkUploadId
                ),
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 2),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: expectedChunkUploadId
                )
            ])
        }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.currentChunks = [expectedChunkUploadId: [preexistingChunk]]

        // With real new item uploads we do not have an associated ItemMetadata as the template is
        // passed onto us by the OS. We cannot rely on the chunkUploadId property we usually use
        // during modified item uploads.
        //
        // We therefore can only use the system-provided item template's itemIdentifier as the
        // chunked upload identifier during new item creation.
        //
        // To test this situation we set the ocId of the metadata used to construct the item
        // template to the chunk upload id.
        var fileItemMetadata = SendableItemMetadata(
            ocId: illegalChunkUploadId, fileName: "file", account: Self.account
        )
        fileItemMetadata.ocId = illegalChunkUploadId
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file")
        let tempData = Data(repeating: 1, count: chunkSize * 3)
        try tempData.write(to: tempUrl)

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItemMaybe, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            forcedChunkSize: chunkSize,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
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
        XCTAssertEqual(remoteItem.data, tempData)
        XCTAssertEqual(
            remoteInterface.completedChunkTransferSize[expectedChunkUploadId],
            Int64(tempData.count) - preexistingChunk.size
        )

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.fileName, fileItemMetadata.fileName)
        XCTAssertEqual(dbItem.fileNameView, fileItemMetadata.fileNameView)
        XCTAssertEqual(dbItem.directory, fileItemMetadata.directory)
        XCTAssertEqual(dbItem.serverUrl, fileItemMetadata.serverUrl)
        XCTAssertEqual(dbItem.ocId, createdItem.itemIdentifier.rawValue)
        XCTAssertNil(dbItem.chunkUploadId)
        XCTAssertTrue(dbItem.downloaded)
        XCTAssertTrue(dbItem.uploaded)
    }

    func testCreateDoesNotPropagateIgnoredFile() async {
        let ignoredMatcher = IgnoredFilesMatcher(ignoreList: ["*.tmp", "/build/"], log: FileProviderLogMock())
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // We'll create a file that matches the ignored pattern
        let parentIdentifier = NSFileProviderItemIdentifier.rootContainer
        let metadata = SendableItemMetadata(
            ocId: "ignored-file-id", fileName: "foo.tmp", account: Self.account
        )
        let itemTemplate = Item(
            metadata: metadata,
            parentItemIdentifier: parentIdentifier,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItem, error) = await Item.create(
            basedOn: itemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            ignoredFiles: ignoredMatcher,
            progress: .init(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        // Assert
        XCTAssertEqual(error as? NSFileProviderError, NSFileProviderError(.excludedFromSync))
        XCTAssertNotNil(createdItem)
        XCTAssertEqual(createdItem?.isUploaded, false)
        XCTAssertEqual(createdItem?.isDownloaded, true)
        XCTAssertTrue(rootItem.children.isEmpty)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: metadata.ocId))
    }

    func testCreateLockFileTriggersRemoteLockInsteadOfUpload() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Setup remote folder and file
        let folderRemote = MockRemoteItem(
            identifier: "folder",
            versionIdentifier: "1",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        let targetFileName = "MyDoc.odt"
        let targetRemote = MockRemoteItem(
            identifier: "folder/\(targetFileName)",
            versionIdentifier: "1",
            name: targetFileName,
            remotePath: folderRemote.remotePath + "/" + targetFileName,
            data: Data("test data".utf8),
            locked: false,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        folderRemote.children = [targetRemote]
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        // Insert folder and target file into DB
        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier, fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        var targetMetadata = SendableItemMetadata(
            ocId: targetRemote.identifier, fileName: targetFileName, account: Self.account
        )
        targetMetadata.serverUrl += "/folder"
        Self.dbManager.addItemMetadata(targetMetadata)

        // Construct the lock file metadata
        let lockFileName = ".~lock.\(targetFileName)#"
        var lockFileMetadata = SendableItemMetadata(
            ocId: "lock-id", fileName: lockFileName, account: Self.account
        )
        lockFileMetadata.serverUrl += "/folder"

        let lockItemTemplate = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (createdItem, error) = await Item.create(
            basedOn: lockItemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNotNil(createdItem)
        XCTAssertEqual(createdItem?.isUploaded, false)
        XCTAssertEqual(createdItem?.isDownloaded, true)
        XCTAssertNil(error)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId))
        XCTAssertTrue(targetRemote.locked)
    }

    func testCreateLockFileUnactionableWithoutCapabilities() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        XCTAssert(remoteInterface.capabilities.contains(##""locking": "1.0","##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""locking": "1.0","##, with: "")

        // Setup remote folder and file
        let folderRemote = MockRemoteItem(
            identifier: "folder",
            versionIdentifier: "1",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        let targetFileName = "MyDoc.odt"
        let targetRemote = MockRemoteItem(
            identifier: "folder/\(targetFileName)",
            versionIdentifier: "1",
            name: targetFileName,
            remotePath: folderRemote.remotePath + "/" + targetFileName,
            data: Data("test data".utf8),
            locked: false,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        folderRemote.children = [targetRemote]
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        // Insert folder and target file into DB
        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier, fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        var targetMetadata = SendableItemMetadata(
            ocId: targetRemote.identifier, fileName: targetFileName, account: Self.account
        )
        targetMetadata.serverUrl += "/folder"
        Self.dbManager.addItemMetadata(targetMetadata)

        // Construct the lock file metadata
        let lockFileName = ".~lock.\(targetFileName)#"
        var lockFileMetadata = SendableItemMetadata(
            ocId: "lock-id", fileName: lockFileName, account: Self.account
        )
        lockFileMetadata.serverUrl += "/folder"

        let lockItemTemplate = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .init(folderMetadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (createdItem, error) = await Item.create(
            basedOn: lockItemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNil(createdItem)
        let unwrappedError = try XCTUnwrap(error) as? NSFileProviderError
        XCTAssertEqual(unwrappedError, NSFileProviderError(.excludedFromSync))
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId))
        XCTAssertFalse(targetRemote.locked)
    }
}
