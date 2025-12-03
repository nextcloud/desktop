//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
@testable import NextcloudFileProviderKitMocks
import NextcloudKit
@testable import TestInterface
import XCTest

final class MockRemoteInterfaceTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    lazy var rootTrashItem = MockRemoteItem.rootTrashItem(account: Self.account)

    override func tearDown() {
        rootItem.children = []
        rootTrashItem.children = []
    }

    func testItemForRemotePath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            versionIdentifier: "a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemB = MockRemoteItem(
            identifier: "b",
            versionIdentifier: "b",
            name: "b",
            remotePath: Self.account.davFilesUrl + "/b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemA_B = MockRemoteItem(
            identifier: "b",
            versionIdentifier: "b",
            name: "b",
            remotePath: Self.account.davFilesUrl + "/a/b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let targetItem = MockRemoteItem(
            identifier: "target",
            versionIdentifier: "target",
            name: "target",
            remotePath: Self.account.davFilesUrl + "/a/b/target",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteInterface.rootItem?.children = [itemA, itemB]
        itemA.parent = remoteInterface.rootItem
        itemB.parent = remoteInterface.rootItem
        itemA.children = [itemA_B]
        itemA_B.parent = itemA
        itemA_B.children = [targetItem]
        targetItem.parent = itemA_B

        XCTAssertEqual(
            remoteInterface.item(
                remotePath: Self.account.davFilesUrl + "/a/b/target", account: Self.account.ncKitAccount
            ), targetItem
        )
    }

    func testItemForRootPath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        XCTAssertEqual(
            remoteInterface.item(remotePath: Self.account.davFilesUrl, account: Self.account.ncKitAccount), rootItem
        )
    }

    func testPathParentPath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let testPath = Self.account.davFilesUrl + "/a/B/c/d"
        let expectedPath = Self.account.davFilesUrl + "/a/B/c"

        XCTAssertEqual(
            remoteInterface.parentPath(path: testPath, account: Self.account), expectedPath
        )
    }

    func testRootPathParentPath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let testPath = Self.account.davFilesUrl + "/"
        let expectedPath = Self.account.davFilesUrl + "/"

        XCTAssertEqual(
            remoteInterface.parentPath(path: testPath, account: Self.account), expectedPath
        )
    }

    func testNameFromPath() throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let testPath = Self.account.davFilesUrl + "/a/b/c/d"
        let expectedName = "d"
        let name = try remoteInterface.name(from: testPath)
        XCTAssertEqual(name, expectedName)
    }

    func testCreateFolder() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let newFolderAPath = Self.account.davFilesUrl + "/A"
        let newFolderA_BPath = Self.account.davFilesUrl + "/A/B/"

        let resultA = await remoteInterface.createFolder(
            remotePath: newFolderAPath, account: Self.account
        )
        XCTAssertEqual(resultA.error, .success)

        let resultA_B = await remoteInterface.createFolder(
            remotePath: newFolderA_BPath, account: Self.account
        )
        XCTAssertEqual(resultA_B.error, .success)

        let itemA = remoteInterface.item(remotePath: newFolderAPath, account: Self.account.ncKitAccount)
        XCTAssertNotNil(itemA)
        XCTAssertEqual(itemA?.name, "A")
        XCTAssertTrue(itemA?.directory ?? false)

        let itemA_B = remoteInterface.item(remotePath: newFolderA_BPath, account: Self.account.ncKitAccount)
        XCTAssertNotNil(itemA_B)
        XCTAssertEqual(itemA_B?.name, "B")
        XCTAssertTrue(itemA_B?.directory ?? false)
    }

    func testUpload() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let fileUrl = URL.temporaryDirectory.appendingPathComponent("file.txt", conformingTo: .text)
        let fileData = Data("Hello, World!".utf8)
        let fileSize = Int64(fileData.count)
        try fileData.write(to: fileUrl)

        let result = await remoteInterface.upload(
            remotePath: Self.account.davFilesUrl + "/file.txt", localPath: fileUrl.path, account: Self.account
        )
        XCTAssertEqual(result.remoteError, .success)

        let remoteItem = remoteInterface.item(
            remotePath: Self.account.davFilesUrl + "/file.txt", account: Self.account.ncKitAccount
        )
        XCTAssertNotNil(remoteItem)

        XCTAssertEqual(remoteItem?.name, "file.txt")
        XCTAssertEqual(remoteItem?.size, fileSize)
        XCTAssertEqual(remoteItem?.data, fileData)
        XCTAssertEqual(result.size, fileSize)

        // TODO: Add test for overwriting existing file
    }

    func testUploadTargetName() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let fileName = UUID().uuidString
        let fileUrl = URL.temporaryDirectory.appendingPathComponent(fileName)
        let fileData = Data("Hello, World!".utf8)
        try fileData.write(to: fileUrl)

        let result = await remoteInterface.upload(
            remotePath: Self.account.davFilesUrl + "/file.txt", localPath: fileUrl.path, account: Self.account
        )
        XCTAssertEqual(result.remoteError, .success)

        let remoteItem = remoteInterface.item(
            remotePath: Self.account.davFilesUrl + "/file.txt", account: Self.account.ncKitAccount
        )
        XCTAssertNotNil(remoteItem)
        let remoteItemIncorrectFileName = remoteInterface.item(
            remotePath: Self.account.davFilesUrl + "/" + fileName, account: Self.account.ncKitAccount
        )
        XCTAssertNil(remoteItemIncorrectFileName)
    }

    func testChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        debugPrint(remoteInterface)

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let chunkSize = 3
        var uploadedChunks = [RemoteFileChunk]()
        let result = await remoteInterface.chunkedUpload(
            localPath: fileUrl.path,
            remotePath: remotePath,
            remoteChunkStoreFolderName: UUID().uuidString,
            chunkSize: chunkSize,
            remainingChunks: [],
            creationDate: .init(),
            modificationDate: .init(),
            account: Self.account,
            options: .init(),
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        let resultChunks = try XCTUnwrap(result.fileChunks)
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.nkError, .success)
        XCTAssertEqual(resultChunks.count, expectedChunkCount)
        XCTAssertNotNil(result.file)
        XCTAssertEqual(result.file?.size, Int64(data.count))

        XCTAssertEqual(uploadedChunks.count, resultChunks.count)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, 1)
        XCTAssertEqual(lastUploadedChunkNameInt, expectedChunkCount)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
        )
    }

    func testResumedChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let chunkSize = 3
        let uploadUuid = UUID().uuidString
        let previousUploadedChunkNum = 1
        let previousUploadedChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: uploadUuid
        )
        let previousUploadedChunks = [previousUploadedChunk]

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        debugPrint(remoteInterface)
        remoteInterface.currentChunks = [uploadUuid: previousUploadedChunks]

        let remotePath = Self.account.davFilesUrl + "/file.txt"

        var uploadedChunks = [RemoteFileChunk]()
        let result = await remoteInterface.chunkedUpload(
            localPath: fileUrl.path,
            remotePath: remotePath,
            remoteChunkStoreFolderName: uploadUuid,
            chunkSize: chunkSize,
            remainingChunks: [
                RemoteFileChunk(
                    fileName: String(2),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: uploadUuid
                ),
                RemoteFileChunk(
                    fileName: String(3),
                    size: Int64(data.count - (chunkSize * 2)),
                    remoteChunkStoreFolderName: uploadUuid
                )
            ],
            creationDate: .init(),
            modificationDate: .init(),
            account: Self.account,
            options: .init(),
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        let resultChunks = try XCTUnwrap(result.fileChunks)
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.nkError, .success)
        XCTAssertEqual(resultChunks.count, expectedChunkCount)
        XCTAssertNotNil(result.file)
        XCTAssertEqual(result.file?.size, Int64(data.count))

        XCTAssertEqual(uploadedChunks.count, resultChunks.count - previousUploadedChunks.count)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, previousUploadedChunkNum + 1)
        XCTAssertEqual(lastUploadedChunkNameInt, previousUploadedChunkNum + 2)
        print(uploadedChunks)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
        )
    }

    func testMove() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemB = MockRemoteItem(
            identifier: "b",
            name: "b",
            remotePath: Self.account.davFilesUrl + "/b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemC = MockRemoteItem(
            identifier: "c",
            name: "c",
            remotePath: Self.account.davFilesUrl + "/a/c",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let targetItem = MockRemoteItem(
            identifier: "target",
            name: "target",
            remotePath: Self.account.davFilesUrl + "/a/c/target",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteInterface.rootItem?.children = [itemA, itemB]
        itemA.parent = remoteInterface.rootItem
        itemA.children = [itemC]
        itemC.parent = itemA
        itemB.parent = remoteInterface.rootItem

        itemC.children = [targetItem]
        targetItem.parent = itemC

        let result = await remoteInterface.move(
            remotePathSource: Self.account.davFilesUrl + "/a/c/target",
            remotePathDestination: Self.account.davFilesUrl + "/b/targetRenamed",
            account: Self.account
        )
        XCTAssertEqual(result.error, .success)
        XCTAssertEqual(itemB.children, [targetItem])
        XCTAssertEqual(itemC.children, [])
        XCTAssertEqual(targetItem.parent, itemB)
        XCTAssertEqual(targetItem.name, "targetRenamed")
        XCTAssertEqual(targetItem.remotePath, Self.account.davFilesUrl + "/b/targetRenamed")
    }

    func testDownload() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        debugPrint(remoteInterface)

        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent("file.txt", conformingTo: .text)

        if !FileManager.default.isWritableFile(atPath: fileUrl.path) {
            print("WARNING: TEMP NOT WRITEABLE. SKIPPING TEST")
            return
        }

        let fileData = Data("Hello, World!".utf8)
        _ = await remoteInterface.upload(
            remotePath: Self.account.davFilesUrl + "/file.txt",
            localPath: fileUrl.path,
            account: Self.account
        )

        let result = await remoteInterface.downloadAsync(
            serverUrlFileName: Self.account.davFilesUrl + "/file.txt",
            fileNameLocalPath: fileUrl.path,
            account: Self.account.ncKitAccount,
            options: .init()
        )

        XCTAssertEqual(result.nkError, .success)

        let downloadedData = try Data(contentsOf: fileUrl)
        XCTAssertEqual(downloadedData, fileData)
    }

    func testEnumerate() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemB = MockRemoteItem(
            identifier: "b",
            name: "b",
            remotePath: Self.account.davFilesUrl + "/b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemC = MockRemoteItem(
            identifier: "c",
            name: "c",
            remotePath: Self.account.davFilesUrl + "/c",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemA_A = MockRemoteItem(
            identifier: "a_a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/a/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemA_B = MockRemoteItem(
            identifier: "a_b",
            name: "b",
            remotePath: Self.account.davFilesUrl + "/a/b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemC_A = MockRemoteItem(
            identifier: "c_a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/c/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemC_A_A = MockRemoteItem(
            identifier: "c_a_a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/c/a/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteInterface.rootItem?.children = [itemA, itemB, itemC]
        itemA.parent = remoteInterface.rootItem
        itemB.parent = remoteInterface.rootItem
        itemC.parent = remoteInterface.rootItem

        itemA.children = [itemA_A, itemA_B]
        itemA_A.parent = itemA
        itemA_B.parent = itemA

        itemC.children = [itemC_A]
        itemC_A.parent = itemC

        itemC_A.children = [itemC_A_A]
        itemC_A_A.parent = itemC_A

        let result = await remoteInterface.enumerate(
            remotePath: Self.account.davFilesUrl, depth: .target, account: Self.account
        )
        XCTAssertEqual(result.error, .success)
        XCTAssertEqual(result.files.count, 1)
        let targetRootFile = result.files.first
        let expectedRoot = remoteInterface.rootItem
        XCTAssertEqual(targetRootFile?.ocId, expectedRoot?.identifier)
        XCTAssertEqual(targetRootFile?.fileName, NextcloudKit.shared.nkCommonInstance.rootFileName) // NextcloudKit gives the root dir this name
        XCTAssertEqual(targetRootFile?.serverUrl, "https://mock.nc.com/remote.php/dav/files/testUserId") // NextcloudKit gives the root dir this url
        XCTAssertEqual(targetRootFile?.date, expectedRoot?.creationDate)
        XCTAssertEqual(targetRootFile?.etag, expectedRoot?.versionIdentifier)

        let resultChildren = await remoteInterface.enumerate(
            remotePath: Self.account.davFilesUrl,
            depth: .targetAndDirectChildren,
            account: Self.account
        )
        XCTAssertEqual(resultChildren.error, .success)
        XCTAssertEqual(resultChildren.files.count, 4)
        XCTAssertEqual(
            resultChildren.files.map(\.ocId),
            [
                remoteInterface.rootItem?.identifier,
                itemA.identifier,
                itemB.identifier,
                itemC.identifier
            ]
        )

        let resultAChildren = await remoteInterface.enumerate(
            remotePath: Self.account.davFilesUrl + "/a",
            depth: .targetAndDirectChildren,
            account: Self.account
        )
        XCTAssertEqual(resultAChildren.error, .success)
        XCTAssertEqual(resultAChildren.files.count, 3)
        XCTAssertEqual(
            resultAChildren.files.map(\.ocId),
            [itemA.identifier, itemA_A.identifier, itemA_B.identifier]
        )

        let resultCChildren = await remoteInterface.enumerate(
            remotePath: Self.account.davFilesUrl + "/c",
            depth: .targetAndDirectChildren,
            account: Self.account
        )
        XCTAssertEqual(resultCChildren.error, .success)
        XCTAssertEqual(resultCChildren.files.count, 2)
        XCTAssertEqual(
            resultCChildren.files.map(\.ocId),
            [itemC.identifier, itemC_A.identifier]
        )

        let resultCRecursiveChildren = await remoteInterface.enumerate(
            remotePath: Self.account.davFilesUrl + "/c",
            depth: .targetAndAllChildren,
            account: Self.account
        )
        XCTAssertEqual(resultCRecursiveChildren.error, .success)
        XCTAssertEqual(resultCRecursiveChildren.files.count, 3)
        XCTAssertEqual(
            resultCRecursiveChildren.files.map(\.ocId),
            [itemC.identifier, itemC_A.identifier, itemC_A_A.identifier]
        )
    }

    func testDelete() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a",
            remotePath: Self.account.davFilesUrl + "/a",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemB = MockRemoteItem(
            identifier: "b",
            name: "b",
            remotePath: Self.account.davFilesUrl + "/b",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemA_C = MockRemoteItem(
            identifier: "c",
            name: "c",
            remotePath: Self.account.davFilesUrl + "/a/c",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemA_C_D = MockRemoteItem(
            identifier: "d",
            name: "d",
            remotePath: Self.account.davFilesUrl + "/a/c/d",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteInterface.rootItem?.children = [itemA, itemB]
        itemA.parent = remoteInterface.rootItem
        itemA.children = [itemA_C]
        itemA_C.parent = itemA
        itemB.parent = remoteInterface.rootItem

        itemA_C.children = [itemA_C_D]
        itemA_C_D.parent = itemA_C

        let itemA_C_predeleteName = itemA_C.name

        let result = await remoteInterface.delete(
            remotePath: Self.account.davFilesUrl + "/a/c", account: Self.account
        )
        XCTAssertEqual(result.error, .success)
        XCTAssertEqual(itemA.children, [])
        XCTAssertEqual(remoteInterface.rootTrashItem?.children.contains(itemA_C), true)
        XCTAssertEqual(itemA_C.name, itemA_C_predeleteName + " (trashed)")
        XCTAssertEqual(itemA_C.remotePath, Self.account.trashUrl + "/c (trashed)")
        XCTAssertEqual(itemA_C_D.trashbinOriginalLocation, "a/c/d")
        XCTAssertEqual(itemA_C_D.remotePath, Self.account.trashUrl + "/c (trashed)/d")
    }

    func testTrashedItems() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a (trashed)",
            remotePath: Self.account.trashUrl + "/a (trashed)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "a"
        )
        let itemB = MockRemoteItem(
            identifier: "b",
            name: "b (trashed)",
            remotePath: Self.account.trashUrl + "/b (trashed)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "b"
        )
        rootTrashItem.children = [itemA, itemB]
        itemA.parent = rootTrashItem
        itemB.parent = rootTrashItem

        let (_, items, _, error) = await remoteInterface.listingTrashAsync(filename: nil, showHiddenFiles: true, account: Self.account.ncKitAccount, options: .init(), taskHandler: { _ in })

        XCTAssertEqual(error, .success)

        let unwrappedItems = try XCTUnwrap(items)
        XCTAssertEqual(unwrappedItems.count, 2)
        XCTAssertEqual(unwrappedItems[0].fileName, "a (trashed)")
        XCTAssertEqual(unwrappedItems[1].fileName, "b (trashed)")
        XCTAssertEqual(unwrappedItems[0].trashbinFileName, "a")
        XCTAssertEqual(unwrappedItems[1].trashbinFileName, "b")
        XCTAssertEqual(unwrappedItems[0].ocId, itemA.identifier)
        XCTAssertEqual(unwrappedItems[1].ocId, itemB.identifier)
    }

    // The server will return ocIds as fileIds. To try to test the item modification steps' handling
    // of this, we intentionally mangle the item's original identifiers while keeping the fileIds
    // consistent (this is what we are able to use to match pre-trashing items with their
    // post-trashing metadata)
    func testTrashingManglesIdentifiers() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        debugPrint(remoteInterface)
        let folderOriginalIdentifier = "folder"
        let folder = MockRemoteItem(
            identifier: folderOriginalIdentifier,
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemAOriginalIdentifier = "a"
        let itemA = MockRemoteItem(
            identifier: itemAOriginalIdentifier,
            name: "a",
            remotePath: Self.account.davFilesUrl + "/a",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [folder]
        folder.parent = rootItem
        folder.children = [itemA]
        itemA.parent = folder

        let (_, _, error) = await remoteInterface.move(
            remotePathSource: folder.remotePath,
            remotePathDestination: Self.account.trashUrl + "/" + folder.name,
            account: Self.account
        )
        XCTAssertEqual(error, .success)
        XCTAssertNotEqual(folder.identifier, folderOriginalIdentifier) // Should not be equal
        XCTAssertEqual(folder.identifier, folderOriginalIdentifier + trashedItemIdSuffix)
        XCTAssertNotEqual(itemA.identifier, itemAOriginalIdentifier) // Should not be equal
        XCTAssertEqual(itemA.identifier, itemAOriginalIdentifier + trashedItemIdSuffix)

        let folderConvertedMetadata = folder.toItemMetadata(account: Self.account)
        XCTAssertEqual(folderConvertedMetadata.fileId, folderOriginalIdentifier)
        let itemAConvertedMetadata = itemA.toItemMetadata(account: Self.account)
        XCTAssertEqual(itemAConvertedMetadata.fileId, itemAOriginalIdentifier)
    }

    func testRestoreFromTrash() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a (trashed)",
            remotePath: Self.account.trashUrl + "/a (trashed)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "a"
        )
        rootTrashItem.children = [itemA]
        itemA.parent = rootTrashItem

        let (_, _, error) =
            await remoteInterface.restoreFromTrash(filename: itemA.name, account: Self.account)
        XCTAssertEqual(error, .success)
        XCTAssertEqual(rootTrashItem.children.count, 0)
        XCTAssertEqual(rootItem.children.count, 1)
        XCTAssertEqual(rootItem.children[0].identifier, "a")
        XCTAssertEqual(itemA.identifier, "a")
        XCTAssertEqual(itemA.remotePath, Self.account.davFilesUrl + "/a")
        XCTAssertEqual(itemA.name, "a")
        XCTAssertNil(itemA.trashbinOriginalLocation)
    }

    func testNoDirectMoveFromTrash() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        debugPrint(remoteInterface)
        let folder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a (trashed)",
            remotePath: Self.account.trashUrl + "/a (trashed)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "a"
        )
        rootTrashItem.children = [itemA]
        itemA.parent = rootTrashItem
        rootItem.children = [folder]
        folder.parent = rootItem

        let newPath = folder.remotePath + "/" + itemA.name
        let (_, _, directMoveError) = await remoteInterface.move(
            remotePathSource: itemA.remotePath,
            remotePathDestination: newPath,
            overwrite: true,
            account: Self.account
        )
        XCTAssertNotEqual(directMoveError, .success) // Should fail as we need to restore first

        let expectedRestoreRemotePath =
            Self.account.davFilesUrl + "/" + itemA.trashbinOriginalLocation!
        let (_, _, restoreError) =
            await remoteInterface.restoreFromTrash(filename: itemA.name, account: Self.account)
        XCTAssertEqual(restoreError, .success)
        XCTAssertEqual(itemA.remotePath, expectedRestoreRemotePath)

        let (_, _, postRestoreMoveError) = await remoteInterface.move(
            remotePathSource: itemA.remotePath,
            remotePathDestination: newPath,
            overwrite: true,
            account: Self.account
        )
        XCTAssertEqual(postRestoreMoveError, .success)
    }

    func testEnforceOverwriteOnRestore() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        debugPrint(remoteInterface)
        let itemA = MockRemoteItem(
            identifier: "a",
            name: "a (trashed)",
            remotePath: Self.account.trashUrl + "/a (trashed)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl,
            trashbinOriginalLocation: "a"
        )
        rootTrashItem.children = [itemA]
        itemA.parent = rootTrashItem

        let restorePath = Self.account.trashRestoreUrl + "/" + itemA.name
        let (_, _, noOverwriteError) = await remoteInterface.move(
            remotePathSource: itemA.remotePath,
            remotePathDestination: restorePath,
            overwrite: false,
            account: Self.account
        )
        XCTAssertNotEqual(noOverwriteError, .success) // Should fail as we enforce overwrite

        let (_, _, overwriteError) = await remoteInterface.move(
            remotePathSource: itemA.remotePath,
            remotePathDestination: restorePath,
            overwrite: true,
            account: Self.account
        )
        XCTAssertEqual(overwriteError, .success)
    }

    func testFetchUserProfile() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)

        let (account, profile, _, error) = await remoteInterface.getUserProfileAsync(
            account: Self.account.ncKitAccount,
            options: .init(),
            taskHandler: { _ in }
        )

        XCTAssertEqual(error, .success)
        XCTAssertEqual(account, Self.account.ncKitAccount)
        XCTAssertNotNil(profile)
    }

    func testTryAuthenticationAttempt() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        debugPrint(remoteInterface)
        let state = await remoteInterface.tryAuthenticationAttempt(account: Self.account)
        XCTAssertEqual(state, .success)
        let failState = await remoteInterface.tryAuthenticationAttempt(
            account: Account(user: "", id: "", serverUrl: "", password: "")
        )
        XCTAssertEqual(failState, .authenticationError)
    }
}
