//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
@testable import TestInterface
import XCTest

///
/// Coverage for `NSFileProviderItem.childItemCount`, where `nil` means nobody has looked and `0`
/// means the directory has been read and is empty.
///
final class ChildItemCountTests: XCTestCase {
    private static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!
    private var remoteInterface: MockRemoteInterface!

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("ChildItemCountTests-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
        remoteInterface = MockRemoteInterface(account: Self.account)
    }

    @discardableResult
    private func seedDirectory(ocId: String, name: String, parentUrl: String) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: name, account: Self.account)
        metadata.directory = true
        metadata.serverUrl = parentUrl
        metadata.uploaded = true
        dbManager.addItemMetadata(metadata)
        return metadata
    }

    private func seedFile(ocId: String, name: String, parentUrl: String) {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: name, account: Self.account)
        metadata.serverUrl = parentUrl
        metadata.uploaded = true
        dbManager.addItemMetadata(metadata)
    }

    private func item(for metadata: SendableItemMetadata) -> Item {
        Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )
    }

    /// A folder whose children have never been read must not claim to be empty, or nothing will
    /// ever ask what is in it.
    func testADirectoryWithNoKnownChildrenReportsUnknownRatherThanZero() {
        let folder = seedDirectory(
            ocId: "unread", name: "2026_Project", parentUrl: Self.account.davFilesUrl
        )

        XCTAssertNil(
            item(for: folder).childItemCount,
            "An unread directory must report nil. Zero is a claim that it is empty."
        )
    }

    /// Once the database does hold children, the count is real knowledge and is reported.
    func testADirectoryWithKnownChildrenReportsHowMany() {
        let folder = seedDirectory(
            ocId: "read", name: "2026_Project", parentUrl: Self.account.davFilesUrl
        )
        let folderUrl = Self.account.davFilesUrl + "/2026_Project"
        seedDirectory(ocId: "c1", name: "00_Input", parentUrl: folderUrl)
        seedDirectory(ocId: "c2", name: "02_Design", parentUrl: folderUrl)
        seedFile(ocId: "c3", name: "brief.pdf", parentUrl: folderUrl)

        XCTAssertEqual(item(for: folder).childItemCount?.intValue, 3)
    }

    /// A file has no children to speak of, and never did.
    func testAFileReportsNoCountAtAll() {
        var file = SendableItemMetadata(ocId: "f", fileName: "brief.pdf", account: Self.account)
        file.serverUrl = Self.account.davFilesUrl
        file.uploaded = true
        dbManager.addItemMetadata(file)

        XCTAssertNil(item(for: file).childItemCount)
    }
}
