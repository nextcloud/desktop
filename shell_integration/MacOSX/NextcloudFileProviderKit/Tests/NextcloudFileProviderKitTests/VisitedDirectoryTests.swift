//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKitMocks
import RealmSwift
@testable import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

///
/// `visitedDirectory` records that a directory has actually been read, which decides both its
/// membership of the working set and whether an empty child count is knowledge or its absence.
///
final class VisitedDirectoryTests: XCTestCase {
    private static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("VisitedDirectoryTests-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
    }

    /// The directory as the server reports it, built once and reused so both reads carry
    /// identical timestamps and the unchanged case is what gets exercised.
    private lazy var folder: SendableItemMetadata = {
        var metadata = SendableItemMetadata(
            ocId: "folder", fileName: "2026_Project", account: Self.account
        )
        metadata.directory = true
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.etag = "unchanged-etag"
        metadata.uploaded = true
        return metadata
    }()

    private func ingest() -> ChangeSet? {
        dbManager.depth1ReadUpdateItemMetadatas(
            account: Self.account.ncKitAccount,
            serverUrl: Self.account.davFilesUrl + "/2026_Project",
            updatedMetadatas: [folder],
            keepExistingDownloadState: true
        )
    }

    /// The regression: the row already exists with the same remote state, and the visit must
    /// still be written.
    func testReadingAnUnchangedDirectoryStillRecordsTheVisit() throws {
        dbManager.addItemMetadata(folder)
        XCTAssertEqual(
            dbManager.itemMetadata(ocId: "folder")?.visitedDirectory, false,
            "Precondition: the seeded row has not been visited."
        )

        _ = ingest()

        XCTAssertEqual(
            dbManager.itemMetadata(ocId: "folder")?.visitedDirectory, true,
            "Reading a directory must record the visit even when nothing remote changed."
        )
    }

    /// The visit is persisted without entering `metadatasToUpdate`, which is also the change set
    /// handed to the framework.
    func testRecordingAVisitDoesNotReportTheDirectoryAsChanged() throws {
        dbManager.addItemMetadata(folder)

        let changes = try XCTUnwrap(ingest())

        XCTAssertFalse(
            changes.updated.contains { $0.ocId == "folder" },
            "A visit is local-only state; it must not be reported to the framework as a change."
        )
        XCTAssertFalse(
            changes.created.contains { $0.ocId == "folder" },
            "The directory already existed; recording its visit must not recreate it."
        )
    }

    /// Once recorded, an empty directory's child count becomes a real zero rather than "unknown".
    func testAVisitedEmptyDirectoryReportsZeroChildrenRatherThanUnknown() throws {
        dbManager.addItemMetadata(folder)
        _ = ingest()

        let metadata = try XCTUnwrap(dbManager.itemMetadata(ocId: "folder"))
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: dbManager
        )

        XCTAssertEqual(
            item.childItemCount?.intValue, 0,
            "A directory that has been read and holds nothing is empty, not unknown."
        )
    }
}
