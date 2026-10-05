//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import TestInterface
import XCTest

///
/// Timing benchmarks for the metadata database, run only when `NCFPK_BENCHMARKS=1` is set.
///
/// These are XCTest cases on purpose: `measure(metrics:options:)` reports median and deviation, which Swift Testing does not offer. They use only the public `FilesDatabaseManager` API and a file-backed database, so the same file runs unchanged against every storage engine and the numbers are comparable across branches.
///
/// Each iteration builds its own fixture outside of the measured interval.
///
final class FilesDatabaseManagerBenchmarkTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "benchUser", id: "benchUserId", serverUrl: "https://bench.nc.com", password: "abcd"
    )

    static let environmentKey = "NCFPK_BENCHMARKS"
    static let iterationCount = 5
    static let flatFolderChildCount = 7000
    static let preservingLocalStateRowCount = 2000
    static let workingSetRowCount = 20000
    static let workingSetChangedRowCount = 500

    override func setUpWithError() throws {
        try super.setUpWithError()
        try XCTSkipUnless(
            ProcessInfo.processInfo.environment[Self.environmentKey] == "1",
            "Benchmarks run only with \(Self.environmentKey)=1."
        )
    }

    private func makeManager() -> FilesDatabaseManager {
        try! FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("benchmark"),
            log: FileProviderLogMock()
        )
    }

    private func makeFile(index: Int, parentUrl: String, etag: String) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: "file-\(index)", fileName: "file-\(index).txt", account: Self.account)
        metadata.serverUrl = parentUrl
        metadata.etag = etag
        metadata.fileId = String(index)
        metadata.uploaded = true
        metadata.size = Int64(index)
        return metadata
    }

    private func makeDirectory(ocId: String, fileName: String, parentUrl: String, etag: String) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: fileName, account: Self.account)
        metadata.directory = true
        metadata.serverUrl = parentUrl
        metadata.etag = etag
        metadata.uploaded = true
        return metadata
    }

    private var measureOptions: XCTMeasureOptions {
        let options = XCTMeasureOptions()
        options.iterationCount = Self.iterationCount
        options.invocationOptions = [.manuallyStart, .manuallyStop]
        return options
    }

    private func report(_ name: String, items: Int, durations: [Duration]) {
        let seconds = durations.map { Double($0.components.seconds) + Double($0.components.attoseconds) / 1e18 }.sorted()
        guard !seconds.isEmpty else {
            return
        }

        let median = seconds[seconds.count / 2]
        let mean = seconds.reduce(0, +) / Double(seconds.count)
        let variance = seconds.reduce(0) { $0 + ($1 - mean) * ($1 - mean) } / Double(seconds.count)
        let itemsPerSecond = median > 0 ? Double(items) / median : 0
        print(String(format: "PERF-BENCH %@ items=%d iterations=%d median_s=%.3f stddev_s=%.3f items_per_s=%.0f", name, items, seconds.count, median, variance.squareRoot(), itemsPerSecond))
    }

    ///
    /// The flat-folder shape that made depth-1 writes quadratic before: one cold pass creating every child, then one warm pass where every etag changed.
    ///
    func testBenchmarkDepth1BatchWriteFlatFolder() {
        let folderUrl = Self.account.davFilesUrl + "/Bench"
        let folder = makeDirectory(ocId: "bench-folder", fileName: "Bench", parentUrl: Self.account.davFilesUrl, etag: "folder-v1")
        var durations: [Duration] = []

        measure(metrics: [XCTClockMetric(), XCTMemoryMetric()], options: measureOptions) {
            let manager = makeManager()
            let cold = [folder] + (0 ..< Self.flatFolderChildCount).map { makeFile(index: $0, parentUrl: folderUrl, etag: "v1") }
            var warmFolder = folder
            warmFolder.etag = "folder-v2"
            let warm = [warmFolder] + (0 ..< Self.flatFolderChildCount).map { makeFile(index: $0, parentUrl: folderUrl, etag: "v2") }

            let clock = ContinuousClock()
            startMeasuring()
            let start = clock.now
            let coldChangeSet = manager.depth1ReadUpdateItemMetadatas(account: Self.account.ncKitAccount, serverUrl: folderUrl, updatedMetadatas: cold, keepExistingDownloadState: true)
            let warmChangeSet = manager.depth1ReadUpdateItemMetadatas(account: Self.account.ncKitAccount, serverUrl: folderUrl, updatedMetadatas: warm, keepExistingDownloadState: true)
            durations.append(clock.now - start)
            stopMeasuring()

            XCTAssertEqual(coldChangeSet?.created.count, Self.flatFolderChildCount + 1)
            XCTAssertEqual(warmChangeSet?.updated.count, Self.flatFolderChildCount + 1)
        }

        report("Depth1BatchWriteFlatFolder", items: 2 * (Self.flatFolderChildCount + 1), durations: durations)
    }

    ///
    /// The paginated ingestion path: every row goes through `addItemMetadataPreservingLocalState` once as a new row and once as a re-ingested row.
    ///
    func testBenchmarkAddItemMetadataPreservingLocalState() {
        let folderUrl = Self.account.davFilesUrl + "/Bench"
        var durations: [Duration] = []

        measure(metrics: [XCTClockMetric(), XCTMemoryMetric()], options: measureOptions) {
            let manager = makeManager()
            let rows = (0 ..< Self.preservingLocalStateRowCount).map { makeFile(index: $0, parentUrl: folderUrl, etag: "v1") }

            let clock = ContinuousClock()
            startMeasuring()
            let start = clock.now
            for row in rows {
                manager.addItemMetadataPreservingLocalState(row)
            }
            for row in rows {
                manager.addItemMetadataPreservingLocalState(row)
            }
            durations.append(clock.now - start)
            stopMeasuring()

            XCTAssertEqual(manager.itemMetadatas(account: Self.account.ncKitAccount).count, Self.preservingLocalStateRowCount)
        }

        report("AddItemMetadataPreservingLocalState", items: 2 * Self.preservingLocalStateRowCount, durations: durations)
    }

    ///
    /// The push-notification lookup: many file identifiers, none of which belong to this domain, against a large table.
    ///
    func testBenchmarkContainsAnyItemMetadata() {
        let folderUrl = Self.account.davFilesUrl + "/Bench"
        let absentIds = Set((1_000_000 ..< 1_005_000).map(String.init))
        var durations: [Duration] = []

        measure(metrics: [XCTClockMetric()], options: measureOptions) {
            let manager = makeManager()
            for index in 0 ..< Self.workingSetRowCount {
                manager.addItemMetadata(makeFile(index: index, parentUrl: folderUrl, etag: "v1"))
            }

            let clock = ContinuousClock()
            startMeasuring()
            let start = clock.now
            let found = manager.containsAnyItemMetadata(fileIds: absentIds)
            durations.append(clock.now - start)
            stopMeasuring()

            XCTAssertFalse(found)
        }

        report("ContainsAnyItemMetadata", items: absentIds.count, durations: durations)
    }

    ///
    /// Writes which carry local-only state and are therefore committed with full synchronization.
    ///
    func testBenchmarkDurableLocalStateWrites() {
        var durations: [Duration] = []

        measure(metrics: [XCTClockMetric()], options: measureOptions) {
            let manager = makeManager()
            let rows = (0 ..< 500).map { makeFile(index: $0, parentUrl: Self.account.davFilesUrl, etag: "v1") }
            for row in rows {
                manager.addItemMetadata(row)
            }

            let clock = ContinuousClock()
            startMeasuring()
            let start = clock.now
            for row in rows {
                _ = try? manager.set(keepDownloaded: true, for: row)
                manager.deleteItemMetadata(ocId: row.ocId)
            }
            durations.append(clock.now - start)
            stopMeasuring()

            XCTAssertEqual(manager.itemMetadata(ocId: "file-0")?.keepDownloaded, true)
        }

        report("DurableLocalStateWrites", items: 1000, durations: durations)
    }

    ///
    /// The working-set scan over a large materialized set where only a few rows changed after the anchor.
    ///
    func testBenchmarkPendingWorkingSetChanges() {
        let folderUrl = Self.account.davFilesUrl + "/Bench"
        let anchor = Date()
        let before = anchor.addingTimeInterval(-3600)
        let after = anchor.addingTimeInterval(60)
        var durations: [Duration] = []

        measure(metrics: [XCTClockMetric(), XCTMemoryMetric()], options: measureOptions) {
            let manager = makeManager()
            for index in 0 ..< Self.workingSetRowCount {
                var row = makeFile(index: index, parentUrl: folderUrl, etag: "v1")
                row.downloaded = true
                row.syncTime = index < Self.workingSetChangedRowCount ? after : before
                manager.addItemMetadata(row)
            }

            let clock = ContinuousClock()
            startMeasuring()
            let start = clock.now
            let changes = manager.pendingWorkingSetChanges(since: anchor)!
            durations.append(clock.now - start)
            stopMeasuring()

            XCTAssertEqual(changes.updated.count, Self.workingSetChangedRowCount)
            XCTAssertEqual(changes.deleted.count, 0)
        }

        report("PendingWorkingSetChanges", items: Self.workingSetRowCount, durations: durations)
    }
}
