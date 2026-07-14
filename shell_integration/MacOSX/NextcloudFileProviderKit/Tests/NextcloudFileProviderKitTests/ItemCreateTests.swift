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

/// Captures the quota XPC calls so tests can verify the extension informs the main app on
/// quota refusals (per-item + per-folder summary). See nextcloud/desktop#9598.
final class QuotaCapturingAppProxy: NSObject, AppProtocol {
    struct ItemCapture: Equatable {
        let fileName: String
        let fileBytes: Int64?
        let availableBytes: Int64?
        let domainIdentifier: String
    }

    var capturedItems: [ItemCapture] = []
    var capturedSummaryDomains: [String] = []

    func presentFileActions(_: String, path _: String, remoteItemPath _: String, withDomainIdentifier _: String) {}
    func openItemInBrowser(_: String, remoteItemPath _: String, forDomainIdentifier _: String) {}
    func copyInternalLink(forItem _: String, remoteItemPath _: String, forDomainIdentifier _: String) {}
    func reportSyncStatus(_: String, forDomainIdentifier _: String) {}
    func reportItemExcluded(fromSync _: String, fileName _: String, reason _: String, forDomainIdentifier _: String) {}

    func reportInsufficientQuota(
        forItem _: String,
        fileName: String,
        fileBytes: NSNumber?,
        availableBytes: NSNumber?,
        forDomainIdentifier domainIdentifier: String
    ) {
        capturedItems.append(
            ItemCapture(
                fileName: fileName,
                fileBytes: fileBytes?.int64Value,
                availableBytes: availableBytes?.int64Value,
                domainIdentifier: domainIdentifier
            )
        )
    }

    func reportInsufficientQuotaSummary(forDomainIdentifier domainIdentifier: String) {
        capturedSummaryDomains.append(domainIdentifier)
    }
}

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

    /// Upload integrity guard (F1): when the server reports it stored a different number of bytes
    /// than the local file contains, `create` must NOT record the item as a clean upload. It
    /// returns a *transient* error (so the File Provider system automatically retries the create)
    /// and best-effort removes the partial remote object, instead of surfacing a torn file as synced.
    func testCreateFileFailsOnUploadSizeMismatch() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("integrity-mismatch-create")
        try Data("Hello world".utf8).write(to: tempUrl)

        // Simulate the server storing fewer bytes than we sent (a torn transfer).
        remoteInterface.uploadResponseSizeOverride = 3

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

        XCTAssertNil(createdItemMaybe)

        // The error must be *transient* (NSCocoaErrorDomain) so the system automatically retries
        // the create rather than backing off until the provider signals resolution.
        let nsError = try XCTUnwrap(error as NSError?)
        XCTAssertEqual(nsError.domain, NSCocoaErrorDomain)
        XCTAssertEqual(nsError.code, NSFileWriteUnknownError)

        // Nothing must be recorded as a clean upload for this item.
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: "file-id"))
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

    /// Regression test for the same root cause as
    /// `testModifyFilePreservesLocalContentModificationDateWhenServerTruncates`,
    /// but on the create path: when the server truncates the upload-response
    /// mtime to 1 s precision, the returned `Item.creationDate` and
    /// `Item.contentModificationDate` must reflect the template's sub-second
    /// values rather than the truncated server response.
    func testCreateFilePreservesLocalDatesWhenServerTruncates() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.uploadResponseMtimeTruncation = 1.0

        let preciseCreationDate = Date(timeIntervalSince1970: 1_747_564_300.123456)
        let preciseMtime = Date(timeIntervalSince1970: 1_747_564_337.456789)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-precise-dates", fileName: "file-precise-dates", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue
        fileItemMetadata.creationDate = preciseCreationDate
        fileItemMetadata.date = preciseMtime

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("file-precise-dates")
        try Data("Hello, precise dates!".utf8).write(to: tempUrl)

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
        XCTAssertNil(error)
        let createdItem = try XCTUnwrap(createdItemMaybe)

        XCTAssertEqual(createdItem.creationDate, preciseCreationDate)
        XCTAssertEqual(createdItem.contentModificationDate, preciseMtime)

        let truncatedMtime = Date(
            timeIntervalSince1970: preciseMtime.timeIntervalSince1970.rounded(.down)
        )
        XCTAssertNotEqual(truncatedMtime, preciseMtime)
        XCTAssertNotEqual(createdItem.contentModificationDate, truncatedMtime)

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdItem.itemIdentifier.rawValue)
        )
        XCTAssertEqual(dbItem.creationDate, preciseCreationDate)
        XCTAssertEqual(dbItem.date, preciseMtime)
    }

    /// Defensive coverage for the secondary fallback: if the system passes a
    /// template whose `creationDate` / `contentModificationDate` are nil (an
    /// `NSFileProviderItem` not built from one of our `Item` instances), the
    /// fix must fall through to the server's response date rather than
    /// silently substituting `Date()` (the previous behaviour, which would
    /// anchor the new metadata to the current wall clock for no good reason).
    func testCreateFileFallsBackToServerDateWhenTemplateHasNilDates() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let template: NSFileProviderItem = MockFileProviderItem(
            identifier: .init("file-nil-dates"),
            filename: "file-nil-dates",
            isUploaded: false
        )
        // MockFileProviderItem doesn't declare creationDate / contentModificationDate, so
        // both flow through the @objc-optional protocol's default-nil. That's exactly the
        // shape this test wants: an item from outside our `Item` type where the system
        // simply didn't furnish those fields.
        XCTAssertNil(template.creationDate ?? nil, "Test setup expects template.creationDate to be nil")
        XCTAssertNil(template.contentModificationDate ?? nil, "Test setup expects template.contentModificationDate to be nil")

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("file-nil-dates")
        try Data("Hello, nil dates!".utf8).write(to: tempUrl)

        let beforeUpload = Date()
        let (createdItemMaybe, error) = await Item.create(
            basedOn: template,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let afterUpload = Date()
        XCTAssertNil(error)
        let createdItem = try XCTUnwrap(createdItemMaybe)

        // The mock echoes the (defaulted-to-now) mtime back as the response,
        // so the resulting dates should equal the server-derived value rather
        // than a second invocation of `Date()` taken after the response. We
        // can't pin to an exact value (the mock initialises with `Date()` when
        // the template hands it nil), but it must land within the upload
        // window — and `creationDate` and `contentModificationDate` must agree
        // with each other.
        let creationDate = try XCTUnwrap(createdItem.creationDate)
        let modificationDate = try XCTUnwrap(createdItem.contentModificationDate)
        XCTAssertEqual(creationDate, modificationDate, "Both dates should derive from the same server response value")
        XCTAssertGreaterThanOrEqual(creationDate, beforeUpload)
        XCTAssertLessThanOrEqual(creationDate, afterUpload)
    }

    /// Pre-flight quota gate: when the parent's `quotaAvailableBytes` is below the local
    /// file size, refuse the upload up-front with `.insufficientQuota` and never call
    /// the remote upload endpoint. See nextcloud/desktop#9598.
    func testCreateFileBlockedByInsufficientQuota() async throws {
        rootItem.quotaAvailableBytes = 4 // less than the file we're about to upload
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("quota-blocked-create")
        try Data("Hello world".utf8).write(to: tempUrl) // 11 bytes > 4 bytes available

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItem, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNil(createdItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .insufficientQuota)
        // No upload was attempted: the file did not appear under the remote root.
        XCTAssertNil(rootItem.children.first { $0.name == fileItemMetadata.fileName })
    }

    /// When an upload is refused by the pre-flight quota gate, the extension must inform the
    /// main app via XPC so it can surface a per-item activity entry plus a per-folder summary
    /// entry with a "Retry all uploads" button. See nextcloud/desktop#9598.
    func testCreateFileRefusedByQuotaReportsToMainApp() async throws {
        rootItem.quotaAvailableBytes = 4
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file-quota-report", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("quota-report-create")
        try Data("Hello world".utf8).write(to: tempUrl) // 11 bytes > 4 bytes available

        let domain = NSFileProviderDomain(
            identifier: NSFileProviderDomainIdentifier("test-domain-create"),
            displayName: "test"
        )
        let proxy = QuotaCapturingAppProxy()

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdItem, error) = await Item.create(
            basedOn: fileItemTemplate,
            contents: tempUrl,
            domain: domain,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            appProxy: proxy,
            log: FileProviderLogMock()
        )

        XCTAssertNil(createdItem)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .insufficientQuota)

        XCTAssertEqual(proxy.capturedItems.count, 1)
        let itemCapture = try XCTUnwrap(proxy.capturedItems.first)
        XCTAssertEqual(itemCapture.fileName, "file-quota-report")
        XCTAssertEqual(itemCapture.domainIdentifier, "test-domain-create")
        XCTAssertEqual(itemCapture.fileBytes, 11)

        XCTAssertEqual(proxy.capturedSummaryDomains, ["test-domain-create"])
    }

    /// Negative `quotaAvailableBytes` (the default for accounts with no quota set) must
    /// not trigger the new pre-flight gate; the upload proceeds normally.
    func testCreateFileWithUnknownQuotaProceeds() async throws {
        XCTAssertEqual(rootItem.quotaAvailableBytes, -1) // default sentinel
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "file-id", fileName: "file-no-quota", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("quota-unknown-create")
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

        XCTAssertNil(error)
        let createdItem = try XCTUnwrap(createdItemMaybe)
        XCTAssertNotNil(rootItem.children.first { $0.identifier == createdItem.itemIdentifier.rawValue })
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

    /// Verify that bundles are refused at the file provider boundary with `.excludedFromSync`
    /// and that nothing is uploaded to the (mock) server. Replaces the previous
    /// `testCreateBundle` test, which validated the now-removed recursive-mirror code path.
    /// See https://github.com/nextcloud/desktop/issues/9827.
    func testCreateBundleIsExcluded() async {
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

        let bundleItemTemplate = Item(
            metadata: bundleItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (item, error) = await Item.create(
            basedOn: bundleItemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        // The exclusion path returns a placeholder item plus the system-recognised exclusion error.
        XCTAssertNotNil(item)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .excludedFromSync)

        // Nothing should have reached the (mock) server.
        XCTAssertNil(rootItem.children.first { $0.name == keynoteBundleFilename })
    }

    /// Same expectation for `.app` (`com.apple.application-bundle`) — historically the most
    /// problematic bundle type for our recursive-mirror approach because of internal symlinks.
    func testCreateDotAppIsExcluded() async {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let appFilename = "Test.app"
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var appMetadata = SendableItemMetadata(
            ocId: "test-app-id", fileName: appFilename, account: Self.account
        )
        appMetadata.directory = true
        appMetadata.serverUrl = Self.account.davFilesUrl
        appMetadata.classFile = NKTypeClassFile.directory.rawValue
        appMetadata.contentType = UTType.applicationBundle.identifier

        let appItemTemplate = Item(
            metadata: appMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (item, error) = await Item.create(
            basedOn: appItemTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertNotNil(item)
        XCTAssertEqual((error as? NSFileProviderError)?.code, .excludedFromSync)
        XCTAssertNil(rootItem.children.first { $0.name == appFilename })
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

    /// An Adobe lock file name does not encode the guarded document's extension, so the document
    /// is resolved by matching a sibling file. Once resolved it is locked on the server just like
    /// for Office lock files, while the lock file itself stays local and is never uploaded.
    func testCreateAdobeInDesignLockFileLocksDocument() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

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

        let targetFileName = "MyDoc.indd"
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

        // InDesign lock file: `~{base name}~{random token}(.idlk`.
        let lockFileName = "~MyDoc~0kjyv(.idlk"
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

        let lockMetadata = Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId)
        XCTAssertNotNil(lockMetadata)
        XCTAssertEqual(lockMetadata?.classFile, "lock")
        XCTAssertEqual(lockMetadata?.isLockFileOfLocalOrigin, true)

        // The lock file itself is never uploaded to the server.
        XCTAssertFalse(folderRemote.children.contains { $0.name == lockFileName })
        // The guarded document is locked on the server.
        XCTAssertTrue(targetRemote.locked)
    }

    /// Premiere Pro lock files are named `{base name}.prlock` and guard a `.prproj` project.
    func testCreateAdobePremiereLockFileLocksDocument() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

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

        let targetFileName = "MyDoc.prproj"
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

        let lockFileName = "MyDoc.prlock"
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
        XCTAssertNil(error)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: lockFileMetadata.ocId)?.isLockFileOfLocalOrigin, true)
        XCTAssertFalse(folderRemote.children.contains { $0.name == lockFileName })
        XCTAssertTrue(targetRemote.locked)
    }

    /// When the guarded document cannot be found (e.g. a stale lock file, or the document is not
    /// in the database), the Adobe lock file is excluded from sync, matching Office behaviour.
    func testCreateAdobeLockFileWithoutDocumentIsExcluded() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

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
        folderRemote.parent = rootItem
        rootItem.children = [folderRemote]

        var folderMetadata = SendableItemMetadata(
            ocId: folderRemote.identifier, fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        Self.dbManager.addItemMetadata(folderMetadata)

        // No `MyDoc.indd` sibling exists in the database.
        let lockFileName = "~MyDoc~0kjyv(.idlk"
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
    }

    ///
    /// A new file created in a folder the user marked "Always keep downloaded"
    /// must inherit that flag so the Finder overlay decoration matches its
    /// siblings; see nextcloud/desktop #10018.
    ///
    func testCreateFileInheritsKeepDownloadedFromPinnedParentFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var folderItemMetadata = SendableItemMetadata(
            ocId: "pinned-folder-id", fileName: "pinned-folder", account: Self.account
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

        // Mimic the user toggling "Always keep downloaded" on the parent folder.
        _ = try Self.dbManager.set(keepDownloaded: true, for: createdFolderItem.metadata)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "child-file-id", fileName: "child.md", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue
        fileItemMetadata.serverUrl = Self.account.davFilesUrl + "/pinned-folder"

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: createdFolderItem.itemIdentifier,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let tempUrl = FileManager.default.temporaryDirectory.appendingPathComponent("child.md")
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
        XCTAssertNil(fileError)
        let createdFileItem = try XCTUnwrap(createdFileItemMaybe)

        XCTAssertTrue(createdFileItem.metadata.keepDownloaded)
        XCTAssertEqual(createdFileItem.contentPolicy, .downloadEagerlyAndKeepDownloaded)
        let decoration = try XCTUnwrap(createdFileItem.decorations?.first)
        XCTAssertTrue(decoration.rawValue.hasSuffix(".keep-downloaded"))

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdFileItem.itemIdentifier.rawValue)
        )
        XCTAssertTrue(dbItem.keepDownloaded)
    }

    ///
    /// Control case for ``testCreateFileInheritsKeepDownloadedFromPinnedParentFolder``:
    /// when the parent is not pinned, the new file must not be pinned either.
    ///
    func testCreateFileDoesNotInheritKeepDownloadedWhenParentNotPinned() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var folderItemMetadata = SendableItemMetadata(
            ocId: "unpinned-folder-id", fileName: "unpinned-folder", account: Self.account
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
        XCTAssertFalse(createdFolderItem.metadata.keepDownloaded)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "unpinned-child-file-id", fileName: "child.md", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue
        fileItemMetadata.serverUrl = Self.account.davFilesUrl + "/unpinned-folder"

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: createdFolderItem.itemIdentifier,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("child-unpinned.md")
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
        XCTAssertNil(fileError)
        let createdFileItem = try XCTUnwrap(createdFileItemMaybe)

        XCTAssertFalse(createdFileItem.metadata.keepDownloaded)
        XCTAssertEqual(createdFileItem.contentPolicy, .inherited)
        XCTAssertNil(createdFileItem.decorations)
    }

    ///
    /// The same inheritance rule applies when the newly-created item is itself
    /// a folder — its descendants will derive their flag from it once the user
    /// drops files inside.
    ///
    func testCreateFolderInheritsKeepDownloadedFromPinnedParentFolder() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var parentMetadata = SendableItemMetadata(
            ocId: "pinned-parent-id", fileName: "pinned-parent", account: Self.account
        )
        parentMetadata.directory = true
        parentMetadata.classFile = NKTypeClassFile.directory.rawValue
        parentMetadata.serverUrl = Self.account.davFilesUrl

        let parentTemplate = Item(
            metadata: parentMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdParentMaybe, parentError) = await Item.create(
            basedOn: parentTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        XCTAssertNil(parentError)
        let createdParent = try XCTUnwrap(createdParentMaybe)

        _ = try Self.dbManager.set(keepDownloaded: true, for: createdParent.metadata)

        var childFolderMetadata = SendableItemMetadata(
            ocId: "child-folder-id", fileName: "child-folder", account: Self.account
        )
        childFolderMetadata.directory = true
        childFolderMetadata.classFile = NKTypeClassFile.directory.rawValue
        childFolderMetadata.serverUrl = Self.account.davFilesUrl + "/pinned-parent"

        let childFolderTemplate = Item(
            metadata: childFolderMetadata,
            parentItemIdentifier: createdParent.itemIdentifier,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let (createdChildFolderMaybe, childFolderError) = await Item.create(
            basedOn: childFolderTemplate,
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        XCTAssertNil(childFolderError)
        let createdChildFolder = try XCTUnwrap(createdChildFolderMaybe)

        XCTAssertTrue(createdChildFolder.metadata.keepDownloaded)
        XCTAssertEqual(createdChildFolder.contentPolicy, .downloadEagerlyAndKeepDownloaded)
        let decoration = try XCTUnwrap(createdChildFolder.decorations?.first)
        XCTAssertTrue(decoration.rawValue.hasSuffix(".keep-downloaded"))

        let dbItem = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdChildFolder.itemIdentifier.rawValue)
        )
        XCTAssertTrue(dbItem.keepDownloaded)
    }

    ///
    /// If the root container itself has been pinned (the user marked "Always
    /// keep downloaded" on the domain's root), top-level new items must also
    /// inherit the flag — this exercises the rootContainer branch of the
    /// parent-resolution code path.
    ///
    func testCreateFileInheritsKeepDownloadedFromPinnedRootContainer() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Seed the root-container row with `keepDownloaded = true`. The
        // ``Item.create`` parent-resolution looks this up via
        // ``FilesDatabaseManager.itemMetadata(ocId:)`` keyed on
        // ``NSFileProviderItemIdentifier.rootContainer``.
        var rootContainerMetadata = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.rootContainer.rawValue,
            fileName: "/",
            account: Self.account
        )
        rootContainerMetadata.directory = true
        rootContainerMetadata.classFile = NKTypeClassFile.directory.rawValue
        rootContainerMetadata.serverUrl = Self.account.davFilesUrl
        rootContainerMetadata.keepDownloaded = true
        Self.dbManager.addItemMetadata(rootContainerMetadata)

        var fileItemMetadata = SendableItemMetadata(
            ocId: "top-level-file-id", fileName: "top-level.md", account: Self.account
        )
        fileItemMetadata.classFile = NKTypeClassFile.document.rawValue
        fileItemMetadata.serverUrl = Self.account.davFilesUrl

        let fileItemTemplate = Item(
            metadata: fileItemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let tempUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("top-level.md")
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
        XCTAssertNil(fileError)
        let createdFileItem = try XCTUnwrap(createdFileItemMaybe)

        XCTAssertTrue(createdFileItem.metadata.keepDownloaded)
        XCTAssertEqual(createdFileItem.contentPolicy, .downloadEagerlyAndKeepDownloaded)
        let decoration = try XCTUnwrap(createdFileItem.decorations?.first)
        XCTAssertTrue(decoration.rawValue.hasSuffix(".keep-downloaded"))
    }
}
