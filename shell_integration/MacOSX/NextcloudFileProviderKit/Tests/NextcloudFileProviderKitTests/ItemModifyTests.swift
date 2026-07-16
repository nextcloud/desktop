//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudFileProviderXPC
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

    /// Real Nextcloud servers echo the upload's `X-OC-Mtime` back via a
    /// `Last-Modified` IMF-fixdate, which collapses to 1 s precision. APFS stores
    /// sub-second mtimes, so if the file provider trusted the server's response
    /// the returned `Item.contentModificationDate` would drift down by hundreds
    /// of milliseconds — and AppKit/NSDocument-based editors (Xcode, TextEdit)
    /// would interpret that drift as "the item has been changed on disk by
    /// another application" right after their own save.
    /// Regression test for that scenario: configure the mock to truncate the
    /// upload response date to 1 s precision, save a sub-second mtime, and
    /// verify the resulting `Item` and the persisted metadata both preserve the
    /// exact local value rather than the truncated one.
    func testModifyFilePreservesLocalContentModificationDateWhenServerTruncates() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.uploadResponseMtimeTruncation = 1.0

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContents = "Hello, New World!".data(using: .utf8)
        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("modify-precise-mtime")
        try newContents?.write(to: newContentsUrl)

        // Pick a sub-second value the mock will truncate down to `…17.000`.
        let preciseLocalMtime = Date(timeIntervalSince1970: 1_747_564_337.456789)
        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
        targetItemMetadata.date = preciseLocalMtime
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
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItemMaybe, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )
        XCTAssertNil(error)
        let modifiedItem = try XCTUnwrap(modifiedItemMaybe)

        XCTAssertEqual(modifiedItem.contentModificationDate, preciseLocalMtime)

        // Belt-and-braces: also confirm we did *not* take the mock server's
        // truncated value. If this is ever equal to the precise value the test
        // is moot, so guard against that too.
        let truncatedResponse = Date(
            timeIntervalSince1970: preciseLocalMtime.timeIntervalSince1970.rounded(.down)
        )
        XCTAssertNotEqual(truncatedResponse, preciseLocalMtime)
        XCTAssertNotEqual(modifiedItem.contentModificationDate, truncatedResponse)

        let persisted = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))
        XCTAssertEqual(persisted.date, preciseLocalMtime)
    }

    /// Modify-path counterpart of `testCreateFileRefusedByQuotaReportsToMainApp`: the extension
    /// must surface a per-item + per-folder summary report to the main app over XPC when an
    /// already-uploaded file is rewritten with content the server can no longer accept.
    func testModifyFileRefusedByQuotaReportsToMainApp() async throws {
        rootItem.quotaAvailableBytes = 4
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContents = "Hello, much bigger New World!".data(using: .utf8)!
        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("quota-report-modify")
        try newContents.write(to: newContentsUrl)

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
        targetItemMetadata.size = Int64(newContents.count)

        let domain = NSFileProviderDomain(
            identifier: NSFileProviderDomainIdentifier("test-domain-modify"),
            displayName: "test"
        )
        let proxy = QuotaCapturingAppProxy()

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

        let (modifiedItem, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            domain: domain,
            dbManager: Self.dbManager,
            appProxy: proxy
        )

        XCTAssertNil(modifiedItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .insufficientQuota)
        XCTAssertEqual(proxy.capturedItems.count, 1)
        XCTAssertEqual(proxy.capturedItems.first?.fileName, "item.txt")
        XCTAssertEqual(proxy.capturedItems.first?.fileBytes, Int64(newContents.count))
        XCTAssertEqual(proxy.capturedSummaryDomains, ["test-domain-modify"])
    }

    /// Pre-flight quota gate on the modify path: when the parent's `quotaAvailableBytes`
    /// is below the new local content size, refuse the upload up-front with
    /// `.insufficientQuota` and leave the remote item untouched.
    /// See nextcloud/desktop#9598.
    func testModifyFileBlockedByInsufficientQuota() async throws {
        rootItem.quotaAvailableBytes = 4 // less than the new content we're about to push
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        let originalRemoteData = remoteItem.data
        let newContents = "Hello, much bigger New World!".data(using: .utf8)!
        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("quota-blocked-modify")
        try newContents.write(to: newContentsUrl)

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
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

        let (modifiedItem, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(modifiedItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .insufficientQuota)
        // The remote bytes must not have been touched by the refused upload.
        XCTAssertEqual(remoteItem.data, originalRemoteData)
    }

    /// Upload integrity guard (F1): when the server reports it stored a different number of bytes
    /// than the local file contains, the modify must NOT record the item as a clean upload. It
    /// returns a *transient* error (so the File Provider system automatically retries the modify)
    /// and leaves the row un-uploaded, instead of committing a truncated/torn file.
    func testModifyFileFailsOnUploadSizeMismatch() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        itemMetadata.uploaded = true
        itemMetadata.downloaded = true
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContents = "Hello, New World!".data(using: .utf8)!
        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("integrity-mismatch-modify")
        try newContents.write(to: newContentsUrl)

        // Simulate the server storing fewer bytes than we sent (a torn transfer).
        remoteInterface.uploadResponseSizeOverride = Int64(newContents.count - 1)

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
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

        let (modifiedItem, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(modifiedItem)

        // The error must be *transient* (NSCocoaErrorDomain, outside the resolvable
        // NSFileProviderError set) so the system automatically retries the modify rather than
        // backing off until the provider signals resolution.
        let nsError = try XCTUnwrap(error as NSError?)
        XCTAssertEqual(nsError.domain, NSCocoaErrorDomain)
        XCTAssertEqual(nsError.code, NSFileWriteUnknownError)

        // The item must not be recorded as a clean upload; it stays pending for the retry.
        let dbItem = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))
        XCTAssertFalse(dbItem.uploaded)
        XCTAssertNotEqual(dbItem.status, Status.normal.rawValue)
    }

    /// When the server returns 404 during an upload (the parent folder was renamed
    /// on another client while the file was open), the extension must:
    ///   - clear the stale lock token so the next attempt goes without an If: header
    ///   - return cannotSynchronize, not noSuchItem — the file still exists at a new path
    ///
    /// Regression test for the race condition described in nextcloud/desktop#9987.
    func testModifyWith404ClearsLockTokenAndReturnsCannotSynchronize() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.uploadError = NKError(statusCode: 404, fallbackDescription: "Not Found")

        var itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        itemMetadata.lockToken = "opaquelocktoken:stale-token-from-old-path"
        itemMetadata.uploaded = true
        itemMetadata.downloaded = true
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("modify-404-test")
        try "Updated content".write(to: newContentsUrl, atomically: true, encoding: .utf8)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let targetItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItem, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(modifiedItem)
        XCTAssertEqual(
            (error as? NSFileProviderError)?.code, .cannotSynchronize,
            "404 during modify must surface as cannotSynchronize, not noSuchItem"
        )
        let updatedMetadata = Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)
        XCTAssertNil(
            updatedMetadata?.lockToken,
            "Lock token must be cleared when the upload path is gone"
        )
    }

    func testModifyWith412ClearsLockToken() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.uploadError = NKError(statusCode: 412, fallbackDescription: "Precondition Failed")

        var itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        itemMetadata.lockToken = "opaquelocktoken:stale-token"
        itemMetadata.uploaded = true
        itemMetadata.downloaded = true
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("modify-412-test")
        try "Updated content".write(to: newContentsUrl, atomically: true, encoding: .utf8)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let targetItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItem, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(modifiedItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .cannotSynchronize)
        let updatedMetadata = Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)
        XCTAssertNil(updatedMetadata?.lockToken, "Stale lock token must be cleared on 412.")
    }

    func testModifyWith423ClearsLockToken() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.uploadError = NKError(statusCode: 423, fallbackDescription: "Locked")

        var itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        itemMetadata.lockToken = "opaquelocktoken:stale-token"
        itemMetadata.uploaded = true
        itemMetadata.downloaded = true
        Self.dbManager.addItemMetadata(itemMetadata)

        let newContentsUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("modify-423-test")
        try "Updated content".write(to: newContentsUrl, atomically: true, encoding: .utf8)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let targetItem = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItem, error) = await item.modify(
            itemTarget: targetItem,
            changedFields: [.contents, .contentModificationDate],
            contents: newContentsUrl,
            dbManager: Self.dbManager
        )

        XCTAssertNil(modifiedItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .cannotSynchronize)
        let updatedMetadata = Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)
        XCTAssertNil(updatedMetadata?.lockToken, "Stale lock token must be cleared on 423.")
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

    /// Verify that a modify operation on a bundle is refused at the file provider boundary
    /// with `.excludedFromSync` and that the (mock) server is left untouched. Replaces the
    /// previous `testModifyBundleContents` test, which validated the now-removed recursive-
    /// mirror code path. See https://github.com/nextcloud/desktop/issues/9827.
    func testModifyBundleIsExcluded() async {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let bundleFilename = "test.key"
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        // Pre-seed: the bundle exists locally + in the DB but never reached the server. This
        // mirrors what would happen after the create-time exclusion path ran on a fresh drag.
        var bundleMetadata = SendableItemMetadata(
            ocId: "test-bundle-id", fileName: bundleFilename, account: Self.account
        )
        bundleMetadata.directory = true
        bundleMetadata.serverUrl = Self.account.davFilesUrl
        bundleMetadata.classFile = NKTypeClassFile.directory.rawValue
        bundleMetadata.contentType = UTType.bundle.identifier
        bundleMetadata.uploaded = false
        bundleMetadata.downloaded = true
        Self.dbManager.addItemMetadata(bundleMetadata)

        let bundleItem = Item(
            metadata: bundleMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (modifiedItem, error) = await bundleItem.modify(
            itemTarget: bundleItem,
            changedFields: [.contents],
            contents: nil,
            dbManager: Self.dbManager
        )

        XCTAssertNotNil(modifiedItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .excludedFromSync)
        // Mock server stays untouched.
        XCTAssertNil(rootItem.children.first { $0.name == bundleFilename })
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
        let newContents = Data(repeating: 1, count: chunkSize * 3)
        let newContentsUrl = FileManager.default.temporaryDirectory.appendingPathComponent("test")
        try newContents.write(to: newContentsUrl)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)

        // The chunk id is derived from (item, size, modificationDate); seed the prior interrupted
        // attempt under exactly that derived id so the resume path recognises identical content.
        let modificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let chunkUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemMetadata.ocId,
            fileSize: Int64(newContents.count),
            modificationDate: modificationDate
        )

        let previousUploadedChunkNum = 1
        let preexistingChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: chunkUploadId
        )
        remoteInterface.currentChunks = [chunkUploadId: [preexistingChunk]]

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

        var targetItemMetadata = SendableItemMetadata(value: itemMetadata)
        targetItemMetadata.date = modificationDate
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
