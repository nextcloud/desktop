//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
@testable import TestInterface
import XCTest

///
/// A row whose folder no longer exists on the server was never cleaned up, so every lookup of it
/// failed again, for example on each framework cache refresh after an update.
///
final class ParentRemoteFallbackTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!
    private var remoteInterface: MockRemoteInterface!
    private var rootItem: MockRemoteItem!

    override func setUp() {
        super.setUp()
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test-\(UUID().uuidString)"),
            log: FileProviderLogMock()
        )
        rootItem = MockRemoteItem.rootItem(account: Self.account)
        remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
    }

    private func storeOrphan(ocId: String, downloaded: Bool = false, directory: Bool = false, visited: Bool = false) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: "\(ocId).indd", account: Self.account)
        metadata.serverUrl = Self.account.davFilesUrl + "/Renamed away"
        metadata.downloaded = downloaded
        metadata.directory = directory
        metadata.visitedDirectory = visited
        dbManager.addItemMetadata(metadata)
        return metadata
    }

    private func parent(of metadata: SendableItemMetadata) async -> NSFileProviderItemIdentifier? {
        await dbManager.parentItemIdentifierWithRemoteFallback(
            fromMetadata: metadata, remoteInterface: remoteInterface, account: Self.account
        )
    }

    func testARowWhoseFolderIsGoneFromTheServerIsMarkedDeleted() async {
        let orphan = storeOrphan(ocId: "orphan")

        let parent = await parent(of: orphan)

        XCTAssertNil(parent)
        XCTAssertEqual(dbManager.itemMetadata(ocId: "orphan")?.deleted, true, "The stale row must not be looked up again.")
    }

    /// A downloaded file is covered by the working-set scan, and dropping it on a single 404 could
    /// race a rename of its folder that the database has not seen yet.
    func testADownloadedFileIsLeftToTheScan() async {
        let orphan = storeOrphan(ocId: "downloaded", downloaded: true)

        _ = await parent(of: orphan)

        XCTAssertEqual(dbManager.itemMetadata(ocId: "downloaded")?.deleted, false)
    }

    func testAVisitedFolderIsLeftToTheScan() async {
        let orphan = storeOrphan(ocId: "visited", directory: true, visited: true)

        _ = await parent(of: orphan)

        XCTAssertEqual(dbManager.itemMetadata(ocId: "visited")?.deleted, false)
    }

    /// Only a 404 says the folder is gone; any other failure may pass.
    func testAnotherReadFailureLeavesTheRowAlone() async {
        let orphan = storeOrphan(ocId: "unreachable")
        remoteInterface.enumerateErrorBySuffix = ["/Renamed away": NKError(statusCode: 503, fallbackDescription: "Service unavailable")]

        _ = await parent(of: orphan)

        XCTAssertEqual(dbManager.itemMetadata(ocId: "unreachable")?.deleted, false)
    }

    func testAFolderThatStillExistsResolvesWithoutTouchingTheRow() async {
        let folder = MockRemoteItem(
            identifier: "folder-id",
            versionIdentifier: "v1",
            name: "Renamed away",
            remotePath: rootItem.remotePath + "/Renamed away",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        folder.parent = rootItem
        rootItem.children.append(folder)
        let child = storeOrphan(ocId: "child")

        let parent = await parent(of: child)

        XCTAssertEqual(parent, NSFileProviderItemIdentifier("folder-id"))
        XCTAssertEqual(dbManager.itemMetadata(ocId: "child")?.deleted, false)
    }
}
