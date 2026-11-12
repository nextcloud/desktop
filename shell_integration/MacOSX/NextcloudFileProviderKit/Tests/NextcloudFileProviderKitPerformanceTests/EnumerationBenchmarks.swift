//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
import TestInterface

///
/// How much work enumeration does: one folder listing with and without server pagination, working-set change passes over a browsed account, and one reconciliation of the items the system reports as materialised.
///
/// The server is ``MockRemoteInterface`` in this process, so the figures cover this package's own work: requests decided, rows written, items reported. Each test checks its counts against ceilings measured on master; see "Performance tests" in the package README for how to change them.
///
@Suite(.serialized)
struct EnumerationBenchmarks {
    // MARK: - Folder listing

    /// Files in the listed folder.
    static let flatFolderFileCount = 1200

    ///
    /// One listing of a flat folder without server pagination, into a database which knows only the folder (cold) or already holds every row (warm).
    ///
    @Test(arguments: [false, true])
    func containerListing(warm: Bool) async throws {
        let counts = try await measureFlatFolderListing(warm: warm, paged: false)

        counts.expectWithin(warm
            ? BenchmarkCounts(requests: 1, commits: 1, rowChanges: 0, reported: 1201)
            : BenchmarkCounts(requests: 1, commits: 1, rowChanges: 1201, reported: 1201))
    }

    ///
    /// The same listing with server pagination, which this package requests from Nextcloud 31 on, in one page larger than the folder.
    ///
    /// A paged listing stores every row in a transaction of its own and reports the folder's children without the folder.
    ///
    @Test(arguments: [false, true])
    func pagedContainerListing(warm: Bool) async throws {
        let counts = try await measureFlatFolderListing(warm: warm, paged: true)

        counts.expectWithin(warm
            ? BenchmarkCounts(requests: 1, commits: 1201, rowChanges: 0, reported: 1200)
            : BenchmarkCounts(requests: 1, commits: 1201, rowChanges: 1201, reported: 1200))
    }

    private func measureFlatFolderListing(warm: Bool, paged: Bool) async throws -> BenchmarkCounts {
        let user = "bench-\(paged ? "paged-" : "")listing-\(warm)"
        let account = Account(user: user, id: user, serverUrl: "https://bench.nc.com", password: "abcd")
        let root = MockRemoteItem.rootItem(account: account)
        let folder = BrowsedTreeFixture.makeItem(name: "Flat", parent: root, directory: true, account: account)

        for index in 0 ..< Self.flatFolderFileCount {
            _ = BrowsedTreeFixture.makeItem(name: "file-\(index).txt", parent: folder, directory: false, account: account)
        }

        let remoteInterface = MockRemoteInterface(account: account, rootItem: root, pagination: paged)

        if paged {
            // This package requests pages only from Nextcloud 31 on, and the mock reports 28.
            remoteInterface.capabilities = remoteInterface.capabilities.replacingOccurrences(of: #""major": 28"#, with: #""major": 31"#)
        }

        let recorder = EnumeratePathRecorder()
        recorder.record(on: remoteInterface)
        let capabilities = try #require(remoteInterface.directMockCapabilities())
        try #require(capabilities.major == (paged ? 31 : 28))
        await RetrievedCapabilitiesActor.shared.setCapabilities(forAccount: account.ncKitAccount, capabilities: capabilities)

        func makeDatabase() throws -> (database: TemporaryDatabase, writes: DatabaseWriteCounter) {
            let database = try TemporaryDatabase(account: account, defaults: Benchmark.defaults)
            _ = database.manager.depth1ReadUpdateItemMetadatas(
                account: account.ncKitAccount,
                serverUrl: root.remotePath,
                updatedMetadatas: [root, folder].map { $0.toItemMetadata(account: account) },
                keepExistingDownloadState: true
            )
            return (database, DatabaseWriteCounter(database.manager))
        }

        // Warm passes share one database, which the warm-ups fill.
        let shared = warm ? try makeDatabase() : nil

        defer {
            shared?.database.remove()
        }

        return try await Benchmark.measure(
            (paged ? "Paged" : "") + (warm ? "ContainerListingWarm" : "ContainerListingCold"),
            prepare: { try shared ?? makeDatabase() },
            counts: { BenchmarkCounts(recorder: recorder, writes: $0.writes) },
            run: { pass in
                // Only a paged listing uses the page size. One page larger than the folder keeps it to one page: for a second page this package sends its encoded page cursor as the server's token, which the mock rejects.
                let observer = try MockEnumerationObserver(enumerator: Enumerator(
                    enumeratedItemIdentifier: NSFileProviderItemIdentifier(folder.identifier),
                    account: account,
                    remoteInterface: remoteInterface,
                    dbManager: pass.database.manager,
                    pageSize: Self.flatFolderFileCount + 1,
                    log: FileProviderLogMock()
                ))
                try await observer.enumerateItems()
                return observer.items.count
            },
            cleanUp: { pass in
                if !warm {
                    pass.database.remove()
                }
            }
        )
    }

    // MARK: - Working-set passes

    ///
    /// The first working-set pass after the database was filled.
    ///
    @Test func workingSetColdPass() async throws {
        let counts = try await Benchmark.measure(
            "WorkingSetColdPass",
            prepare: { try await BrowsedTreeFixture(scenario: "cold-pass") },
            counts: { $0.counts },
            run: { fixture in
                let observer = try await fixture.runWorkingSetPass()
                return observer.changedItems.count + observer.deletedItemIdentifiers.count
            },
            cleanUp: { $0.database.remove() }
        )

        counts.expectWithin(BenchmarkCounts(requests: 21, commits: 26, rowChanges: 18, reported: 5))
    }

    ///
    /// A pass when nothing changed on the server, starting where the previous pass ended, as the system chains them.
    ///
    @Test func workingSetIdlePass() async throws {
        let fixture = try await BrowsedTreeFixture(scenario: "idle-pass")

        defer {
            fixture.database.remove()
        }

        let counts = try await Benchmark.measure(
            "WorkingSetIdlePass",
            prepare: {
                try await Benchmark.waitForNextSecond()
                return fixture
            },
            counts: { $0.counts },
            run: { fixture in
                let observer = try await fixture.runWorkingSetPass()
                return observer.changedItems.count + observer.deletedItemIdentifiers.count
            }
        )

        counts.expectWithin(BenchmarkCounts(requests: 21, commits: 26, rowChanges: 18, reported: 5))
    }

    ///
    /// Like the idle pass, but one file deep in the tree changed on the server before each pass.
    ///
    @Test func workingSetPassWithOneChangedFile() async throws {
        let fixture = try await BrowsedTreeFixture(scenario: "changed-file-pass")

        defer {
            fixture.database.remove()
        }

        let counts = try await Benchmark.measure(
            "WorkingSetPassWithOneChangedFile",
            prepare: {
                fixture.changeFileOnServer()
                try await Benchmark.waitForNextSecond()
                return fixture
            },
            counts: { $0.counts },
            run: { fixture in
                let observer = try await fixture.runWorkingSetPass()
                #expect(observer.changedItems.contains { $0.itemIdentifier.rawValue == fixture.changedFile.identifier }, "The changed file must be reported.")
                return observer.changedItems.count + observer.deletedItemIdentifiers.count
            }
        )

        counts.expectWithin(BenchmarkCounts(requests: 22, commits: 27, rowChanges: 33, reported: 10))
    }

    ///
    /// A pass after a folder with files appeared on the server inside a listed folder.
    ///
    /// Inside a pinned folder the pass also reads the new folder, whose files inherit the pin; elsewhere the new folder is only reported, and its files are read when it is opened.
    ///
    @Test(arguments: [false, true])
    func workingSetPassWithNewFolder(pinned: Bool) async throws {
        let counts = try await Benchmark.measure(
            pinned ? "WorkingSetPassWithNewFolderPinned" : "WorkingSetPassWithNewFolder",
            prepare: {
                let fixture = try await BrowsedTreeFixture(scenario: "new-folder-\(pinned)")
                try fixture.addFolderOnServer(pinningItsParent: pinned)
                return fixture
            },
            counts: { $0.counts },
            run: { fixture in
                let observer = try await fixture.runWorkingSetPass()
                #expect(fixture.recorder.paths.contains { $0.hasSuffix("/New") } == pinned, "Only a pinned parent has the new folder read in the same pass.")
                return observer.changedItems.count + observer.deletedItemIdentifiers.count
            },
            cleanUp: { $0.database.remove() }
        )

        counts.expectWithin(pinned
            ? BenchmarkCounts(requests: 23, commits: 28, rowChanges: 49, reported: 15)
            : BenchmarkCounts(requests: 22, commits: 27, rowChanges: 33, reported: 10))
    }

    // MARK: - Materialised set

    ///
    /// One reconciliation of the items the system reports as materialised, after it evicted the downloads in unlisted folders and before it caught up with five files downloaded just now.
    ///
    /// Its reported count is the items the pass hands back to the extension as materialised or evicted; the extension then refreshes their ancestors.
    ///
    @Test func materializationReconciliationPass() async throws {
        typealias Pass = (fixture: BrowsedTreeFixture, reported: Set<NSFileProviderItemIdentifier>, orphans: Set<NSFileProviderItemIdentifier>)

        let counts = try await Benchmark.measure(
            "MaterializationReconciliationPass",
            prepare: { () async throws -> Pass in
                let fixture = try await BrowsedTreeFixture(scenario: "materialization-pass")

                do {
                    let manager = fixture.database.manager
                    let downloads = try #require(fixture.changedFile.parent).children

                    for file in downloads {
                        let item = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier(file.identifier), account: fixture.account, remoteInterface: fixture.remoteInterface, dbManager: manager, log: FileProviderLogMock()))
                        let (url, _, error) = await item.fetchContents(dbManager: manager)
                        try #require(error == nil)
                        try FileManager.default.removeItem(at: #require(url))
                    }

                    let materialised = try #require(manager.materialisedItemMetadatas(account: fixture.account.ncKitAccount))
                    let orphans = Set(materialised.filter { $0.fileName.hasPrefix("orphan-") }.map { NSFileProviderItemIdentifier($0.ocId) })
                    let reported = Set(materialised.map { NSFileProviderItemIdentifier($0.ocId) })
                        .subtracting(orphans)
                        .subtracting(downloads.map { NSFileProviderItemIdentifier($0.identifier) })

                    return (fixture, reported, orphans)
                } catch {
                    // Benchmark.measure cleans up only after prepare returned.
                    fixture.database.remove()
                    throw error
                }
            },
            counts: { $0.fixture.counts },
            run: { pass in
                let manager = pass.fixture.database.manager
                let observer = MaterializedEnumerationObserver(account: pass.fixture.account, dbManager: manager, log: FileProviderLogMock()) { _, _ in }

                let (materialized, evicted) = await withCheckedContinuation { continuation in
                    observer.handleEnumeratedItems(pass.reported, account: pass.fixture.account, dbManager: manager) { materialized, evicted in
                        continuation.resume(returning: (materialized, evicted))
                    }
                }

                #expect(materialized.isEmpty, "Every reported item is already stored as materialised.")
                #expect(evicted == pass.orphans, "Exactly the downloads in unlisted folders are evicted; the fresh downloads wait for the system.")
                return materialized.count + evicted.count
            },
            cleanUp: { $0.fixture.database.remove() }
        )

        counts.expectWithin(BenchmarkCounts(requests: 0, commits: 1, rowChanges: 5, reported: 5))
    }
}
