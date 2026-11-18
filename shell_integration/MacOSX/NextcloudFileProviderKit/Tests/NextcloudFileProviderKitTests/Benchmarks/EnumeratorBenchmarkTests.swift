//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import TestInterface
import XCTest

///
/// Request-count and timing benchmarks for the working-set change scan, run only when `NCFPK_BENCHMARKS=1` is set.
///
/// The fixture is the shape a browsed account has: a tree of visited directories three levels deep, files in every leaf, a downloaded file in some leaves, and downloaded files whose own directory was never visited. The mock server answers without latency, so the number of PROPFINDs a pass issues is the figure which carries over to a real server; the wall time covers the local work of a pass.
///
final class EnumeratorBenchmarkTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "benchUser", id: "benchUserId", serverUrl: "https://bench.nc.com", password: "abcd"
    )

    static let iterationCount = 3
    static let branchingFactor = 10
    static let filesPerLeaf = 5
    static let downloadedLeafStride = 10
    static let unvisitedDirectoryCount = 50

    override func setUpWithError() throws {
        try super.setUpWithError()
        try XCTSkipUnless(
            ProcessInfo.processInfo.environment[FilesDatabaseManagerBenchmarkTests.environmentKey] == "1",
            "Benchmarks run only with \(FilesDatabaseManagerBenchmarkTests.environmentKey)=1."
        )
    }

    private struct Fixture {
        let remoteInterface: MockRemoteInterface
        let dbManager: FilesDatabaseManager
        let recorder: EnumeratePathRecorder
        let changedLeafFile: MockRemoteItem
        let visitedDirectories: [MockRemoteItem]
        let itemCount: Int
    }

    private func makeItem(name: String, parent: MockRemoteItem, directory: Bool, data: Data? = nil) -> MockRemoteItem {
        let item = MockRemoteItem(
            identifier: name,
            versionIdentifier: "v1",
            name: name,
            remotePath: parent.remotePath + "/" + name,
            directory: directory,
            data: data,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        item.parent = parent
        parent.children.append(item)
        return item
    }

    /// Store a directory listing the way a working-set scan does, which also marks the directory visited.
    private func list(_ directory: MockRemoteItem, in dbManager: FilesDatabaseManager) {
        let files = [directory] + directory.children
        _ = dbManager.depth1ReadUpdateItemMetadatas(
            account: Self.account.ncKitAccount,
            serverUrl: directory.remotePath,
            updatedMetadatas: files.map { $0.toItemMetadata(account: Self.account) },
            keepExistingDownloadState: true
        )
    }

    private func makeFixture() -> Fixture {
        let rootItem = MockRemoteItem.rootItem(account: Self.account)
        var visitedDirectories: [MockRemoteItem] = []
        var downloadedFiles: [MockRemoteItem] = []
        var itemCount = 0
        var changedLeafFile: MockRemoteItem?

        for a in 0 ..< Self.branchingFactor {
            let level1 = makeItem(name: "L1-\(a)", parent: rootItem, directory: true)
            visitedDirectories.append(level1)
            itemCount += 1

            for b in 0 ..< Self.branchingFactor {
                let level2 = makeItem(name: "L2-\(a)-\(b)", parent: level1, directory: true)
                visitedDirectories.append(level2)
                itemCount += 1

                for c in 0 ..< Self.branchingFactor {
                    let leaf = makeItem(name: "Leaf-\(a)-\(b)-\(c)", parent: level2, directory: true)
                    visitedDirectories.append(leaf)
                    itemCount += 1

                    for f in 0 ..< Self.filesPerLeaf {
                        let file = makeItem(name: "file-\(a)-\(b)-\(c)-\(f).txt", parent: leaf, directory: false, data: Data([UInt8(f)]))
                        itemCount += 1

                        if f == 0, (a * Self.branchingFactor * Self.branchingFactor + b * Self.branchingFactor + c) % Self.downloadedLeafStride == 0 {
                            downloadedFiles.append(file)
                        }

                        if a == Self.branchingFactor / 2, b == Self.branchingFactor / 2, c == Self.branchingFactor / 2, f == 1 {
                            changedLeafFile = file
                        }
                    }
                }
            }
        }

        // Downloaded files reached by identifier, whose own directory the user never listed.
        var unvisitedDirectoryFiles: [MockRemoteItem] = []
        for u in 0 ..< Self.unvisitedDirectoryCount {
            let directory = makeItem(name: "Unvisited-\(u)", parent: rootItem, directory: true)
            let file = makeItem(name: "orphan-\(u).bin", parent: directory, directory: false, data: Data([1, 2, 3]))
            unvisitedDirectoryFiles.append(file)
            itemCount += 2
        }

        let dbManager = try! FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("benchmark"),
            log: FileProviderLogMock()
        )

        list(rootItem, in: dbManager)
        for directory in visitedDirectories {
            list(directory, in: dbManager)
        }
        for file in downloadedFiles + unvisitedDirectoryFiles {
            var metadata = file.toItemMetadata(account: Self.account)
            metadata.downloaded = true
            dbManager.addItemMetadata(metadata)
        }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        recorder.record(on: remoteInterface)

        return Fixture(
            remoteInterface: remoteInterface, dbManager: dbManager, recorder: recorder, changedLeafFile: changedLeafFile!,
            visitedDirectories: visitedDirectories, itemCount: itemCount
        )
    }

    /// One working-set change pass from `anchor`; returns the anchor it finished with.
    private func runPass(_ fixture: Fixture, from anchor: NSFileProviderSyncAnchor) async throws -> NSFileProviderSyncAnchor {
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: fixture.remoteInterface,
            dbManager: fixture.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: anchor)
        XCTAssertNil(observer.error)
        return try XCTUnwrap(observer.finishes.last?.anchor)
    }

    ///
    /// Three passes over the same fixture: the first after the database was filled, an idle pass, and a pass after one file in one leaf changed.
    ///
    /// The request counts are asserted, not only reported: an idle pass costs the root listing alone, and one changed leaf file costs the listings on its path from the root.
    ///
    /// Timed with `ContinuousClock` and reported through ``BenchmarkReport`` rather than `measure(metrics:)`, because the passes are asynchronous.
    ///
    func testBenchmarkWorkingSetScanPasses() async throws {
        var coldDurations: [Duration] = []
        var idleDurations: [Duration] = []
        var changedDurations: [Duration] = []
        var coldRequests = 0
        var idleRequests = 0
        var changedRequests = 0
        var itemCount = 0
        let clock = ContinuousClock()

        for _ in 0 ..< Self.iterationCount {
            let fixture = makeFixture()
            itemCount = fixture.itemCount
            var anchor = Enumerator.syncAnchor(at: Date(), storeIdentity: fixture.dbManager.storeIdentity)

            var start = clock.now
            anchor = try await runPass(fixture, from: anchor)
            coldDurations.append(clock.now - start)
            coldRequests = fixture.recorder.count

            fixture.recorder.reset()
            start = clock.now
            anchor = try await runPass(fixture, from: anchor)
            idleDurations.append(clock.now - start)
            idleRequests = fixture.recorder.count

            fixture.changedLeafFile.changeVersion(to: "v2")
            fixture.recorder.reset()
            start = clock.now
            let enumerator = try Enumerator(
                enumeratedItemIdentifier: .workingSet,
                account: Self.account,
                remoteInterface: fixture.remoteInterface,
                dbManager: fixture.dbManager,
                log: FileProviderLogMock()
            )
            let observer = MockChangeObserver(enumerator: enumerator)
            try await observer.enumerateChanges(from: anchor)
            changedDurations.append(clock.now - start)
            changedRequests = fixture.recorder.count

            XCTAssertNil(observer.error)
            XCTAssertTrue(
                observer.changedItems.contains { $0.itemIdentifier.rawValue == fixture.changedLeafFile.identifier },
                "The changed leaf file must be reported."
            )
        }

        XCTAssertEqual(idleRequests, 1, "An idle domain costs one request, the root listing.")
        XCTAssertEqual(changedRequests, 4, "One changed leaf file costs the listings of the root and the three directories on its path.")

        BenchmarkReport.print("WorkingSetScanColdPass", items: itemCount, durations: coldDurations, requests: coldRequests)
        BenchmarkReport.print("WorkingSetScanIdlePass", items: itemCount, durations: idleDurations, requests: idleRequests)
        BenchmarkReport.print("WorkingSetScanOneChangedLeaf", items: itemCount, durations: changedDurations, requests: changedRequests)
    }

    ///
    /// The passes after the system itself enumerated every visited directory, as Finder does on a tree walk.
    ///
    /// Each enumeration reconciles its directory when nothing below it is pending, so the first pass lists the root, the ancestors of the downloaded files, and the directories the system never listed but which hold a downloaded file; the pass after that lists the root alone. One iteration: the walk dominates and its cost is reported too.
    ///
    func testBenchmarkWorkingSetScanAfterSystemWalk() async throws {
        let fixture = makeFixture()
        let clock = ContinuousClock()

        var start = clock.now
        for directory in fixture.visitedDirectories {
            let enumerator = try Enumerator(
                enumeratedItemIdentifier: NSFileProviderItemIdentifier(directory.identifier),
                account: Self.account,
                remoteInterface: fixture.remoteInterface,
                dbManager: fixture.dbManager,
                log: FileProviderLogMock()
            )
            try await MockEnumerationObserver(enumerator: enumerator).enumerateItems()
        }
        let walkDuration = clock.now - start

        fixture.recorder.reset()
        start = clock.now
        let anchor = try await runPass(fixture, from: Enumerator.syncAnchor(at: Date(), storeIdentity: fixture.dbManager.storeIdentity))
        let firstDuration = clock.now - start
        let firstRequests = fixture.recorder.count

        fixture.recorder.reset()
        start = clock.now
        _ = try await runPass(fixture, from: anchor)
        let idleDuration = clock.now - start
        let idleRequests = fixture.recorder.count

        // Every level-1 and level-2 directory had a downloaded file pending below it when the system listed it, so only the leaves were reconciled by the walk.
        let ancestorsOfDownloads = Self.branchingFactor + Self.branchingFactor * Self.branchingFactor
        XCTAssertEqual(firstRequests, 1 + ancestorsOfDownloads + Self.unvisitedDirectoryCount, "The root, the ancestors of the downloads, and the directories holding a download which the system never listed.")
        XCTAssertEqual(idleRequests, 1, "Everything the system or the scan listed is reconciled.")

        BenchmarkReport.print("SystemWalkOfVisitedDirectories", items: fixture.visitedDirectories.count, durations: [walkDuration], requests: fixture.visitedDirectories.count)
        BenchmarkReport.print("WorkingSetScanFirstPassAfterSystemWalk", items: fixture.itemCount, durations: [firstDuration], requests: firstRequests)
        BenchmarkReport.print("WorkingSetScanIdlePassAfterSystemWalk", items: fixture.itemCount, durations: [idleDuration], requests: idleRequests)
    }
}
