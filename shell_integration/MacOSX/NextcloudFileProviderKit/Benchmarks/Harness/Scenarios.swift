//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit

///
/// Name of the directory `Benchmarks/provision.sh` creates on the server.
///
let fixtureRootName = "nfpk-bench"

/// Directory inside the fixture holding one wide, flat listing.
let fixtureWideDirectoryName = "wide"

/// Directory inside the fixture holding the deep, many-container tree.
let fixtureTreeDirectoryName = "tree"

enum Scenario: String, CaseIterable {
    case workingSetScan = "working-set-scan"
    case workingSetScanWithChange = "working-set-scan-change"
    case itemEnumerationCold = "item-enumeration-cold"
    case itemEnumerationRepeat = "item-enumeration-repeat"

    var summary: String {
        switch self {
            case .workingSetScan:
                "Full working-set walk over the materialised tree with nothing changed on the server."
            case .workingSetScanWithChange:
                "Full working-set walk with one file changed on the server, measuring how soon it is reported."
            case .itemEnumerationCold:
                "First enumeration of a wide directory into an empty database."
            case .itemEnumerationRepeat:
                "Second enumeration of the same unchanged directory, measuring redundant persistence."
        }
    }
}

struct ScenarioRunner {
    let environment: BenchmarkEnvironment
    let revision: String
    let repetitions: Int

    private var account: Account {
        environment.account
    }

    private var dbManager: FilesDatabaseManager {
        environment.dbManager
    }

    private var remoteInterface: CountingRemoteInterface {
        environment.remoteInterface
    }

    private var fixtureRoot: String {
        account.davFilesUrl + "/" + fixtureRootName
    }

    private var wideDirectory: String {
        fixtureRoot + "/" + fixtureWideDirectoryName
    }

    private var treeDirectory: String {
        fixtureRoot + "/" + fixtureTreeDirectoryName
    }

    func run(_ scenario: Scenario) async throws -> BenchmarkResult {
        switch scenario {
            case .workingSetScan: try await runWorkingSetScan(changingAFile: false)
            case .workingSetScanWithChange: try await runWorkingSetScan(changingAFile: true)
            case .itemEnumerationCold: try await runItemEnumeration(repeated: false)
            case .itemEnumerationRepeat: try await runItemEnumeration(repeated: true)
        }
    }

    // MARK: - Working-set walk

    ///
    /// Materialise the fixture tree, then time one working-set change enumeration over it.
    ///
    /// This is the whole path a remote-change signal drives inside the extension: the scan reads
    /// every materialised container against the server, persists what it finds, and reports the
    /// changes to the observer. Everything below the observer is production code talking to a real
    /// Nextcloud.
    ///
    private func runWorkingSetScan(changingAFile: Bool) async throws -> BenchmarkResult {
        let materialised = try await materialiseTree()

        var result = BenchmarkResult(
            scenario: changingAFile ? Scenario.workingSetScanWithChange.rawValue
                : Scenario.workingSetScan.rawValue,
            revision: revision,
            metrics: []
        )
        result.record("materialised items", materialised, unit: "items", lowerIsBetter: false)

        var durations = [Double]()
        var firstChangeDurations = [Double]()
        var enumerations = 0
        var maxConcurrent = 0
        var reportedChanges = 0

        for repetition in 0 ..< repetitions {
            if changingAFile {
                try await touchOneFixtureFile(iteration: repetition)
            }

            remoteInterface.reset()

            let enumerator = try Enumerator(
                enumeratedItemIdentifier: .workingSet,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                log: BenchmarkLog()
            )

            let clock = ContinuousClock()
            let startedAt = clock.now
            let observer = BenchmarkChangeObserver(enumerator: enumerator, startedAt: startedAt)
            let anchor = Enumerator.syncAnchor(at: Date(timeIntervalSinceNow: -300))

            try await observer.enumerateChanges(from: anchor)
            let elapsed = startedAt.duration(to: clock.now)

            durations.append(elapsed.seconds)
            if let timeToFirstChange = observer.timeToFirstChange {
                firstChangeDurations.append(timeToFirstChange.seconds)
            }

            let traffic = remoteInterface.traffic
            enumerations = traffic.enumerations
            maxConcurrent = traffic.maxConcurrentEnumerations
            reportedChanges = observer.updatedItems.count
        }

        result.record("walk duration", median(durations), unit: "s")
        result.record("PROPFINDs issued", enumerations, unit: "requests")
        result.record("peak concurrent PROPFINDs", maxConcurrent, unit: "requests", lowerIsBetter: false)
        result.record("changes reported", reportedChanges, unit: "items", lowerIsBetter: false)

        if !firstChangeDurations.isEmpty {
            let timeToFirst = median(firstChangeDurations)
            result.record("time to first change", timeToFirst, unit: "s")
            let walk = median(durations)
            if walk > 0 {
                result.record(
                    "first change at", timeToFirst / walk * 100, unit: "% of walk"
                )
            }
        }

        return result
    }

    ///
    /// Populate the database from the server and mark the fixture tree materialised.
    ///
    /// Directories qualify through `visitedDirectory` and files through `downloaded`, which is what
    /// puts them in the working set the scan walks. The rows are stamped well in the past so a later
    /// pass that rewrites one is visible as a moved `syncTime`.
    ///
    @discardableResult
    private func materialiseTree() async throws -> Int {
        // Descend the whole fixture tree, not just its top level: the walk under measurement reads a
        // materialised directory to cover the files inside it, so those files have to exist locally
        // for the walk to have the shape it has on a real domain.
        try await enumerateContainer(remotePath: treeDirectory)

        var enumerated = Set<String>([treeDirectory])

        while true {
            let pending = dbManager
                .itemMetadatas(account: account.ncKitAccount)
                .filter {
                    $0.directory
                        && $0.remotePath().hasPrefix(treeDirectory)
                        && !enumerated.contains($0.remotePath())
                }

            guard !pending.isEmpty else { break }

            for directory in pending {
                try await enumerateContainer(remotePath: directory.remotePath())
                enumerated.insert(directory.remotePath())
            }
        }

        var materialised = 0

        for var metadata in dbManager.itemMetadatas(account: account.ncKitAccount) {
            guard metadata.remotePath().hasPrefix(treeDirectory) else { continue }

            if metadata.directory {
                metadata.visitedDirectory = true
            } else {
                metadata.downloaded = true
            }

            metadata.syncTime = seedSyncTime
            dbManager.addItemMetadata(metadata)
            materialised += 1
        }

        return materialised
    }

    // MARK: - Item enumeration

    ///
    /// Enumerate one wide directory, optionally after an identical enumeration has already filled
    /// the database.
    ///
    /// The repeat pass is where redundant persistence shows up: the listing is unchanged, so the
    /// correct number of rows to write is zero. `rows rewritten` counts the rows whose `syncTime`
    /// moved, which only a real write does.
    ///
    private func runItemEnumeration(repeated: Bool) async throws -> BenchmarkResult {
        var result = BenchmarkResult(
            scenario: repeated ? Scenario.itemEnumerationRepeat.rawValue
                : Scenario.itemEnumerationCold.rawValue,
            revision: revision,
            metrics: []
        )

        if repeated {
            try await enumerateContainer(remotePath: wideDirectory)
            stampAllRows(with: seedSyncTime)
        }

        remoteInterface.reset()

        let clock = ContinuousClock()
        let startedAt = clock.now
        let observer = try await enumerateContainer(remotePath: wideDirectory)
        let elapsed = startedAt.duration(to: clock.now)

        let traffic = remoteInterface.traffic
        result.record("enumeration duration", elapsed)
        result.record("items reported", observer.items.count, unit: "items", lowerIsBetter: false)
        result.record("pages", observer.pageCount, unit: "pages", lowerIsBetter: false)
        result.record("PROPFINDs issued", traffic.enumerations, unit: "requests")

        if repeated {
            result.record("rows rewritten", rowsRewrittenSinceSeed(), unit: "rows")
            result.record(
                "rows in database",
                dbManager.itemMetadatas(account: account.ncKitAccount).count,
                unit: "rows",
                lowerIsBetter: false
            )
        }

        return result
    }

    // MARK: - Helpers

    /// Marker the fixture rows are stamped with. Any row whose `syncTime` is later than this was
    /// written by the pass under measurement.
    private var seedSyncTime: Date {
        Date(timeIntervalSince1970: 1_000_000_000)
    }

    ///
    /// Resolve a remote path to the identifier the enumerator needs, enumerating its ancestors first
    /// when the database has not seen it yet.
    ///
    /// A container is addressed by ocId, and an ocId only exists locally once something has listed
    /// the parent. Walking down from the account root is what the extension does when a user opens a
    /// folder for the first time.
    ///
    private func resolveContainer(remotePath: String) async throws -> NSFileProviderItemIdentifier {
        if remotePath == account.davFilesUrl {
            return .rootContainer
        }

        if let ocId = knownOcId(forRemotePath: remotePath) {
            return NSFileProviderItemIdentifier(ocId)
        }

        guard let separator = remotePath.lastIndex(of: "/") else {
            throw BenchmarkError.fixtureMissing(path: remotePath)
        }

        let parentPath = String(remotePath[..<separator])
        try await enumerateContainer(remotePath: parentPath)

        guard let ocId = knownOcId(forRemotePath: remotePath) else {
            throw BenchmarkError.fixtureMissing(path: remotePath)
        }

        return NSFileProviderItemIdentifier(ocId)
    }

    /// Match on the same string the scan itself builds, rather than re-parsing the URL.
    private func knownOcId(forRemotePath remotePath: String) -> String? {
        dbManager
            .itemMetadatas(account: account.ncKitAccount)
            .first { $0.remotePath() == remotePath && !$0.deleted }?
            .ocId
    }

    @discardableResult
    private func enumerateContainer(remotePath: String) async throws -> BenchmarkEnumerationObserver {
        let identifier = try await resolveContainer(remotePath: remotePath)

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: identifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: BenchmarkLog()
        )

        let observer = BenchmarkEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        return observer
    }

    private func stampAllRows(with date: Date) {
        for var metadata in dbManager.itemMetadatas(account: account.ncKitAccount) {
            metadata.syncTime = date
            dbManager.addItemMetadata(metadata)
        }
    }

    private func rowsRewrittenSinceSeed() -> Int {
        dbManager
            .itemMetadatas(account: account.ncKitAccount)
            .count { $0.syncTime > seedSyncTime }
    }

    ///
    /// Change one fixture file on the server so the walk has something real to discover.
    ///
    private func touchOneFixtureFile(iteration: Int) async throws {
        let target = treeDirectory + "/dir-000/changed-\(iteration).txt"
        let payload = "changed at iteration \(iteration)\n"

        let localUrl = FileManager.default.temporaryDirectory
            .appendingPathComponent("nfpk-bench-change-\(iteration).txt")
        try payload.write(to: localUrl, atomically: true, encoding: .utf8)
        defer { try? FileManager.default.removeItem(at: localUrl) }

        let (_, _, _, _, _, _, error) = await remoteInterface.upload(
            remotePath: target,
            localPath: localUrl.path,
            creationDate: nil,
            modificationDate: nil,
            account: account,
            options: .init(),
            requestHandler: { _ in },
            taskHandler: { _ in },
            progressHandler: { _ in }
        )

        guard error == .success else {
            throw BenchmarkError.serverUnreachable(
                url: target, detail: "Could not change a fixture file: \(error.errorDescription)"
            )
        }
    }
}
