//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest

///
/// Uses XCTest because `FilesDatabaseManager` reads Realm's process-wide default configuration, which Swift Testing suites running in parallel replace.
///
final class RootContainerExcludeListTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    var rootItem: MockRemoteItem!
    var remoteInterface: MockRemoteInterface!
    var globalIgnoredFiles: IgnoredFilesMatcher!
    /// An in-memory Realm loses its contents when its last instance is released.
    private var keepAliveRealm: Realm?

    override func setUp() {
        super.setUp()
        // The manager's initializer replaces Realm's default configuration.
        _ = Self.dbManager
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        keepAliveRealm = Self.dbManager.ncDatabase()
        rootItem = MockRemoteItem.rootItem(account: Self.account)
        remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        globalIgnoredFiles = IgnoredFilesMatcher(ignoreList: ["*.part"], log: FileProviderLogMock())
    }

    override func tearDown() {
        rootItem.children = []
        keepAliveRealm = nil
        super.tearDown()
    }

    ///
    /// Put an exclude list file into a folder on the server and into the local database, as enumeration would.
    ///
    /// - Parameters:
    ///     - parent: The folder to put the file into, or the root container if `nil`.
    ///
    @discardableResult
    private func addExcludeList(_ contents: String, etag: String, parent: MockRemoteItem? = nil) -> MockRemoteItem {
        let parent = parent ?? rootItem!
        let excludeList = MockRemoteItem(
            identifier: "exclude-list-" + parent.identifier,
            versionIdentifier: etag,
            name: ".sync-exclude.lst",
            remotePath: parent.remotePath + "/.sync-exclude.lst",
            data: Data(contents.utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        parent.children.append(excludeList)
        excludeList.parent = parent
        Self.dbManager.addItemMetadata(excludeList.toItemMetadata(account: Self.account))

        return excludeList
    }

    private func resolveMatcher(_ excludeList: RootContainerExcludeList) async throws -> IgnoredFilesMatcher {
        try await excludeList.matcher(extending: globalIgnoredFiles, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager)
    }

    func testWithoutExcludeListOnlyGlobalPatternsApply() async throws {
        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        XCTAssertTrue(matcher === globalIgnoredFiles)
        XCTAssertEqual(remoteInterface.downloadCount, 0)
    }

    func testExcludeListAppliesToTheWholeDomainWithGlobalPatterns() async throws {
        addExcludeList("*.excl\nfoo/bar\nbuild/\n", etag: "1")

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        XCTAssertTrue(matcher.isExcluded("a.excl"))
        XCTAssertTrue(matcher.isExcluded("deep/nested/a.excl"))
        XCTAssertTrue(matcher.isExcluded("foo/bar/file.txt"))
        XCTAssertFalse(matcher.isExcluded("other/foo/bar"))
        XCTAssertTrue(matcher.isExcluded("sub/build", isDirectory: true))
        XCTAssertTrue(matcher.isExcluded("sub/c.part"))
        XCTAssertFalse(matcher.isExcluded("a.txt"))
        XCTAssertFalse(matcher.isExcluded(".sync-exclude.lst"))
    }

    func testExcludeListInSubfolderIsNotRead() async throws {
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
        rootItem.children.append(folder)
        folder.parent = rootItem
        Self.dbManager.addItemMetadata(folder.toItemMetadata(account: Self.account))
        addExcludeList("*.excl\n", etag: "1", parent: folder)

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        XCTAssertFalse(matcher.isExcluded("folder/a.excl"))
        XCTAssertEqual(remoteInterface.downloadCount, 0)
    }

    func testDeletedExcludeListIsIgnored() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        var metadata = excludeList.toItemMetadata(account: Self.account)
        metadata.deleted = true
        Self.dbManager.addItemMetadata(metadata)

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        XCTAssertFalse(matcher.isExcluded("a.excl"))
        XCTAssertEqual(remoteInterface.downloadCount, 0)
    }

    func testExcludeListIsDownloadedAgainOnlyWhenItsEtagChanges() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())

        var matcher = try await resolveMatcher(rootContainerExcludeList)
        XCTAssertTrue(matcher.isExcluded("a.excl"))
        matcher = try await resolveMatcher(rootContainerExcludeList)
        XCTAssertTrue(matcher.isExcluded("a.excl"))
        XCTAssertEqual(remoteInterface.downloadCount, 1)

        excludeList.data = Data("*.other\n".utf8)
        excludeList.versionIdentifier = "2"
        Self.dbManager.addItemMetadata(excludeList.toItemMetadata(account: Self.account))

        matcher = try await resolveMatcher(rootContainerExcludeList)
        XCTAssertFalse(matcher.isExcluded("a.excl"))
        XCTAssertTrue(matcher.isExcluded("a.other"))
        XCTAssertEqual(remoteInterface.downloadCount, 2)
    }

    func testUploadedExcludeListIsUsedWithoutDownload() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("*.uploaded\n".utf8).write(to: contents)
        defer { try? FileManager.default.removeItem(at: contents) }
        excludeList.versionIdentifier = "2"
        let uploadedMetadata = excludeList.toItemMetadata(account: Self.account)

        await rootContainerExcludeList.didUpload(uploadedMetadata, contents: contents, account: Self.account)
        Self.dbManager.addItemMetadata(uploadedMetadata)
        let matcher = try await resolveMatcher(rootContainerExcludeList)

        XCTAssertTrue(matcher.isExcluded("a.uploaded"))
        XCTAssertFalse(matcher.isExcluded("a.excl"))
        XCTAssertEqual(remoteInterface.downloadCount, 0)
    }

    func testUploadOfOtherItemsDoesNotChangeExcludeList() async throws {
        addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("*.other\n".utf8).write(to: contents)
        defer { try? FileManager.default.removeItem(at: contents) }
        var otherMetadata = SendableItemMetadata(ocId: "other", fileName: ".sync-exclude.lst", account: Self.account)
        otherMetadata.serverUrl = Self.account.davFilesUrl + "/folder"
        otherMetadata.etag = "1"

        await rootContainerExcludeList.didUpload(otherMetadata, contents: contents, account: Self.account)
        let matcher = try await resolveMatcher(rootContainerExcludeList)

        XCTAssertTrue(matcher.isExcluded("a.excl"))
        XCTAssertFalse(matcher.isExcluded("a.other"))
        XCTAssertEqual(remoteInterface.downloadCount, 1)
    }

    func testFailsWithRetryableErrorWhenExcludeListCannotBeDownloaded() async throws {
        addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        let cases: [(NKError, NSFileProviderError.Code)] = [
            (NKError(statusCode: 503, fallbackDescription: "Service Unavailable"), .serverUnreachable),
            (NKError(statusCode: 401, fallbackDescription: "Unauthorized"), .notAuthenticated),
            (NKError(statusCode: 403, fallbackDescription: "Forbidden"), .cannotSynchronize)
        ]

        for (downloadError, expectedCode) in cases {
            remoteInterface.downloadError = downloadError

            do {
                _ = try await resolveMatcher(rootContainerExcludeList)
                XCTFail("Expected the exclude list to fail closed for status \(downloadError.errorCode).")
            } catch {
                XCTAssertEqual((error as? NSFileProviderError)?.code, expectedCode)
            }
        }
    }

    func testExcludeListMissingOnServerExcludesNothing() async throws {
        addExcludeList("*.excl\n", etag: "1")
        remoteInterface.downloadError = NKError(statusCode: 404, fallbackDescription: "Not Found")

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        XCTAssertFalse(matcher.isExcluded("a.excl"))
        XCTAssertTrue(matcher.isExcluded("a.part"))
    }

    func testUsesEarlierVersionWhenCurrentCannotBeDownloaded() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        _ = try await resolveMatcher(rootContainerExcludeList)

        excludeList.versionIdentifier = "2"
        Self.dbManager.addItemMetadata(excludeList.toItemMetadata(account: Self.account))
        remoteInterface.downloadError = NKError(statusCode: 503, fallbackDescription: "Service Unavailable")
        let matcher = try await resolveMatcher(rootContainerExcludeList)

        XCTAssertTrue(matcher.isExcluded("a.excl"))
        XCTAssertEqual(remoteInterface.downloadCount, 2)
    }

    func testConcurrentCallersShareOneDownload() async throws {
        addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        // Keep the download in progress long enough for every caller to arrive.
        remoteInterface.downloadCallHandler = {
            try? await Task.sleep(nanoseconds: 200_000_000)
        }
        let sharedIgnoredFiles = try XCTUnwrap(globalIgnoredFiles)
        let sharedRemoteInterface = try XCTUnwrap(remoteInterface)

        let results = try await withThrowingTaskGroup(of: Bool.self) { group in
            for _ in 0 ..< 20 {
                group.addTask {
                    try await rootContainerExcludeList
                        .matcher(extending: sharedIgnoredFiles, account: Self.account, remoteInterface: sharedRemoteInterface, dbManager: Self.dbManager)
                        .isExcluded("a.excl")
                }
            }

            return try await group.reduce(into: [Bool]()) { $0.append($1) }
        }

        XCTAssertEqual(results, Array(repeating: true, count: 20))
        XCTAssertEqual(remoteInterface.downloadCount, 1)
    }

    func testCreateInSubfolderIsNotUploadedWhenExcludedByExcludeList() async throws {
        addExcludeList("*.excl\n", etag: "1")
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
        rootItem.children.append(folder)
        folder.parent = rootItem
        Self.dbManager.addItemMetadata(folder.toItemMetadata(account: Self.account))
        var metadata = SendableItemMetadata(ocId: "excluded-file", fileName: "a.excl", account: Self.account)
        metadata.serverUrl = folder.remotePath
        let itemTemplate = Item(
            metadata: metadata,
            parentItemIdentifier: .init(folder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("Hello world".utf8).write(to: contents)
        defer { try? FileManager.default.removeItem(at: contents) }
        let ignoredFiles = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        let (createdItem, error) = await Item.create(
            basedOn: itemTemplate,
            contents: contents,
            account: Self.account,
            remoteInterface: remoteInterface,
            ignoredFiles: ignoredFiles,
            progress: .init(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(error as? NSFileProviderError, NSFileProviderError(.excludedFromSync))
        XCTAssertEqual(createdItem?.isUploaded, false)
        XCTAssertTrue(folder.children.isEmpty)
    }
}
