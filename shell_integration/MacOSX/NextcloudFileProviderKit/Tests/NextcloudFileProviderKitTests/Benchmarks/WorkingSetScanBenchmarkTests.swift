//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import TestInterface
import XCTest

///
/// Timing benchmarks for a full working-set walk against the mock server, which fail when its cost grows more than 1.5 times as fast as the account.
///
final class WorkingSetScanBenchmarkTests: BenchmarkTestCase {
    static let smallFolderCount = 250
    static let largeFolderCount = 2000

    /// Room for noise above the growth of a linear walk, which is the ratio of the folder counts.
    static let noiseHeadroom = 1.5
    static let maximumGrowth = Double(largeFolderCount) / Double(smallFolderCount) * noiseHeadroom

    func testBenchmarkFullWalkOfUnchangedVisitedFolders() async throws {
        try await benchmarkFullWalk("FullWalkOfUnchangedVisitedFolders", changed: false)
    }

    func testBenchmarkFullWalkOfChangedVisitedFolders() async throws {
        try await benchmarkFullWalk("FullWalkOfChangedVisitedFolders", changed: true)
    }

    ///
    /// Time full walks of one visited folder listing visited subfolders that each hold one downloaded file, at two sizes.
    ///
    /// - Parameter changed: Whether every folder has a new etag on the server, so the walk takes the changed-folder path instead of the unchanged one.
    ///
    private func benchmarkFullWalk(_ name: String, changed: Bool) async throws {
        let small = try await fullWalkDurations(folderCount: Self.smallFolderCount, changed: changed)
        let large = try await fullWalkDurations(folderCount: Self.largeFolderCount, changed: changed)

        let smallMedian = try XCTUnwrap(report("\(name)-\(Self.smallFolderCount)", items: 1 + 2 * Self.smallFolderCount, durations: small))
        let largeMedian = try XCTUnwrap(report("\(name)-\(Self.largeFolderCount)", items: 1 + 2 * Self.largeFolderCount, durations: large))

        XCTAssertLessThan(largeMedian / smallMedian, Self.maximumGrowth)
    }

    private func fullWalkDurations(folderCount: Int, changed: Bool) async throws -> [Duration] {
        var durations = [Duration]()

        for _ in 0 ..< Self.iterationCount {
            let dbManager = try FilesDatabaseManager(
                account: Self.account,
                databaseDirectory: makeDatabaseDirectory(),
                fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("benchmark"),
                log: FileProviderLogMock()
            )
            let rootItem = MockRemoteItem.rootItem(account: Self.account)
            let bench = makeItem(identifier: "bench", name: "Bench", parent: rootItem, directory: true)
            var folders = [MockRemoteItem]()
            var rows = [bench.toItemMetadata(account: Self.account)]

            for index in 0 ..< folderCount {
                let folder = makeItem(identifier: "folder-\(index)", name: "folder-\(index)", parent: bench, directory: true)
                let file = makeItem(identifier: "file-\(index)", name: "file.txt", parent: folder, directory: false)
                folders.append(folder)
                rows.append(folder.toItemMetadata(account: Self.account))
                rows.append(file.toItemMetadata(account: Self.account))
            }

            for var row in rows {
                row.visitedDirectory = row.directory
                row.downloaded = !row.directory
                dbManager.addItemMetadata(row)
            }

            if changed {
                for item in [bench] + folders {
                    item.versionIdentifier = "v2"
                }
            }

            let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
            let enumerator = try Enumerator(
                enumeratedItemIdentifier: .workingSet,
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: FileProviderLogMock()
            )

            let clock = ContinuousClock()
            let start = clock.now
            let changes = await enumerator.scanMaterialisedItemsForRemoteChanges()
            durations.append(clock.now - start)

            XCTAssertFalse(changes.hadFailure)
            XCTAssertTrue(changes.deleted.isEmpty)
            XCTAssertEqual(changes.updated.count, changed ? 1 + folderCount : 0)
            XCTAssertEqual(remoteInterface.readOperationCount, 1 + folderCount)
        }

        return durations
    }

    private func makeItem(identifier: String, name: String, parent: MockRemoteItem, directory: Bool) -> MockRemoteItem {
        let item = MockRemoteItem(
            identifier: identifier,
            versionIdentifier: "v1",
            name: name,
            remotePath: parent.remotePath + "/" + name,
            directory: directory,
            data: directory ? nil : Data([1]),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        item.parent = parent
        parent.children.append(item)
        return item
    }
}
