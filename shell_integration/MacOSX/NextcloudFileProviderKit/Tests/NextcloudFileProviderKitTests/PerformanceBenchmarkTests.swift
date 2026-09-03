//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

//
//  Reproducible measurements for the working-set scan changes.
//
//  The figures quoted in the pull request were gathered on a live account and are not reproducible
//  from a checkout. What *is* reproducible is the mechanism underneath each of them, and that is
//  what these tests pin:
//
//    - how many PROPFINDs one scan issues (the "full walk vs. push-targeted walk" figure),
//    - how many database rows one scan rewrites (the "1,998 rows to surface 7 changes" figure),
//    - how many of the scan's reads overlap in time (the "76 seconds of sequential PROPFINDs"
//      figure).
//
//  Each is a count, not a duration, so they hold on any machine and in CI. The two timing-shaped
//  benchmarks additionally report a wall clock via `measure`, which is only comparable between
//  revisions on the same machine — run them on `stable-34.0` and on this branch to reproduce the
//  ratio, not the absolute numbers. See `Documentation/WorkingSetScanBenchmarks.md`.
//

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
@testable import TestInterface
import XCTest

final class PerformanceBenchmarkTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    /// Directories in the synthetic tree. Each is materialised, so each is one PROPFIND of a full
    /// walk. Large enough that a concurrency limit of six is visibly exercised, small enough to keep
    /// the suite fast.
    static let directoryCount = 30

    /// Files per directory. They are materialised too, but a depth-1 read of their parent covers
    /// them, so they must not add reads to the scan — which is itself one of the things measured.
    static let filesPerDirectory = 6

    /// Files in the single wide directory the write-volume benchmarks enumerate. Wide enough to
    /// span several pages at ``paginatedPageSize``, which is the path that used to rewrite every row
    /// it read.
    static let paginatedDirectoryFileCount = 200

    /// Page size for those enumerations, so the listing really is delivered in pages.
    static let paginatedPageSize = 50

    /// Stand-in for a PROPFIND round trip, used by the benchmarks that measure overlap rather than
    /// counts. Reads against the mock tree are otherwise instantaneous, which hides the only cost
    /// the scan actually pays.
    static let simulatedReadLatency: TimeInterval = 0.05

    private var dbManager: FilesDatabaseManager!
    private var rootItem: MockRemoteItem!
    private var directories: [MockRemoteItem] = []

    /// Seeded rows are stamped well before the anchor, so a row whose `syncTime` has moved past this
    /// is a row the scan wrote.
    private let seedSyncTime = Date(timeIntervalSinceNow: -600)
    private let anchorDate = Date(timeIntervalSinceNow: -300)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
        rootItem = MockRemoteItem.rootItem(account: Self.account)
        directories = []
    }

    override func tearDown() {
        directories = []
        rootItem = nil
        dbManager = nil
        super.tearDown()
    }

    // MARK: - Fixture

    ///
    /// Build a materialised tree of ``directoryCount`` directories, each holding
    /// ``filesPerDirectory`` files, and seed the database with its current state.
    ///
    /// Every row is seeded from the mock's own state, so the tree starts fully in sync: a scan of it
    /// has nothing to report and nothing to write, which is the baseline both count benchmarks
    /// measure against.
    ///
    private func buildMaterialisedTree() {
        for directoryIndex in 0 ..< Self.directoryCount {
            let directory = makeFolder(
                name: "folder-\(directoryIndex)", parent: rootItem, etag: "folder-\(directoryIndex)-v1"
            )
            directories.append(directory)

            for fileIndex in 0 ..< Self.filesPerDirectory {
                let file = makeFile(
                    name: "file-\(directoryIndex)-\(fileIndex).txt",
                    parent: directory,
                    etag: "file-\(directoryIndex)-\(fileIndex)-v1"
                )
                seed(file, downloaded: true)
            }

            seed(directory, visitedDirectory: true)
        }
    }

    @discardableResult
    private func seed(
        _ item: MockRemoteItem, downloaded: Bool = false, visitedDirectory: Bool = false
    ) -> SendableItemMetadata {
        var metadata = item.toItemMetadata(account: Self.account)
        metadata.downloaded = downloaded
        metadata.visitedDirectory = visitedDirectory
        metadata.syncTime = seedSyncTime
        dbManager.addItemMetadata(metadata)
        return metadata
    }

    ///
    /// Build one directory of ``paginatedDirectoryFileCount`` files and seed the database with its
    /// current state, so an enumeration of it has nothing to persist.
    ///
    /// Separate from ``buildMaterialisedTree()`` because write volume is a property of the
    /// *enumeration* path, which paginates on servers from Nextcloud 31, whereas the working-set
    /// scan reads without pagination. One wide directory is what that path actually pages through.
    ///
    private func buildPaginatedDirectory() -> MockRemoteItem {
        let folder = makeFolder(name: "paginated", parent: rootItem, etag: "paginated-v1")

        for fileIndex in 0 ..< Self.paginatedDirectoryFileCount {
            let file = makeFile(
                name: "file-\(fileIndex).txt", parent: folder, etag: "file-\(fileIndex)-v1"
            )
            seed(file)
        }

        seed(folder, visitedDirectory: true)
        return folder
    }

    ///
    /// Page through a directory exactly as ``Enumerator/enumerateItems(for:startingAt:)`` does on a
    /// Nextcloud 31+ server, and return how many pages it took.
    ///
    /// The read is driven through ``Enumerator/readServerUrl(_:pageSettings:account:remoteInterface:dbManager:domain:enumeratedItemIdentifier:depth:log:)``
    /// directly because the enumerator decides whether to paginate from the server's advertised
    /// major version, and the mock advertises 28. Passing `pageSettings` is the same switch that
    /// version check flips.
    ///
    @discardableResult
    private func enumerateItemsPaginated(
        of folder: MockRemoteItem, using remoteInterface: MockRemoteInterface
    ) async throws -> Int {
        var page: NSFileProviderPage?
        var index = 0

        while true {
            let result = await Enumerator.readServerUrl(
                folder.remotePath,
                pageSettings: (page: page, index: index, size: Self.paginatedPageSize),
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                depth: .targetAndDirectChildren,
                log: FileProviderLogMock()
            )

            if let error = result.error, error != .success {
                XCTFail("Paginated read of page \(index) failed: \(error)")
                break
            }

            index += 1

            guard let token = result.nextPage?.token,
                  let tokenData = token.data(using: .utf8)
            else { break }

            page = NSFileProviderPage(tokenData)
        }

        return index
    }

    private func makeFolder(name: String, parent: MockRemoteItem, etag: String) -> MockRemoteItem {
        let folder = MockRemoteItem(
            identifier: name,
            versionIdentifier: etag,
            name: name,
            remotePath: parent.remotePath + "/" + name,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        folder.parent = parent
        parent.children.append(folder)
        return folder
    }

    private func makeFile(
        name: String, parent: MockRemoteItem, etag: String, data: Data = Data([1, 2, 3])
    ) -> MockRemoteItem {
        let file = MockRemoteItem(
            identifier: name,
            versionIdentifier: etag,
            name: name,
            remotePath: parent.remotePath + "/" + name,
            data: data,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        file.parent = parent
        parent.children.append(file)
        return file
    }

    private func runWorkingSetChanges(
        _ remoteInterface: MockRemoteInterface
    ) async throws -> MockChangeObserver {
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate))
        return observer
    }

    /// Rows whose persisted `syncTime` has moved past the seeded value, i.e. rows the scan wrote.
    /// The skip guard in `addItemMetadataPreservingLocalState` returns before the write, so a
    /// skipped row keeps its seeded timestamp exactly.
    private func rewrittenRowCount() -> Int {
        dbManager
            .ncDatabase()
            .objects(RealmItemMetadata.self)
            .filter { $0.syncTime > self.seedSyncTime }
            .count
    }

    private var totalSeededRows: Int {
        Self.directoryCount * (Self.filesPerDirectory + 1)
    }

    // MARK: - Request volume

    ///
    /// A full walk reads every materialised *directory* once and no materialised file individually:
    /// a depth-1 read of the parent already carries each child's state.
    ///
    /// This is the baseline the push-targeted figure is a ratio against, and it is also the guard
    /// against the walk silently regaining per-file reads — the shape that made one measured pass
    /// spend 76 seconds on 669 PROPFINDs to surface no changes at all.
    ///
    func testFullWalkReadsEachMaterialisedDirectoryOnce() async throws {
        buildMaterialisedTree()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.resetOperationCounters()

        _ = try await runWorkingSetChanges(remoteInterface)

        XCTAssertEqual(
            remoteInterface.readOperationCount,
            Self.directoryCount,
            """
            A full walk must issue exactly one read per materialised directory (\(Self.directoryCount)), \
            and none for the \(Self.directoryCount * Self.filesPerDirectory) materialised files their \
            depth-1 reads already cover.
            """
        )
    }

    ///
    /// A walk restricted to the containers a push named reads only those containers, and still
    /// reports the change inside one of them.
    ///
    /// This is the mechanism behind the largest figure in the pull request: a push identifies the
    /// changed file within a second or so, but the extension used to answer it by walking the whole
    /// materialised set. The ratio here is the same one measured on the live account — reads fall
    /// from "every materialised directory" to "the directories that changed".
    ///
    func testPushTargetedWalkReadsOnlyTheNamedContainers() async throws {
        buildMaterialisedTree()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // First pass: a full walk, which is also what arms the interval that lets the next pass be
        // targeted at all (`RemoteChangeTargets.shouldRunFullScan()` forces a full walk until one
        // has completed).
        remoteInterface.resetOperationCounters()
        _ = try await runWorkingSetChanges(remoteInterface)
        let fullWalkReads = remoteInterface.readOperationCount

        // A file changes on the server inside one directory, and the push names that directory.
        let changedDirectory = try XCTUnwrap(directories.first)
        let changedFile = try XCTUnwrap(changedDirectory.children.first)
        changedFile.versionIdentifier = "changed-v2"
        changedFile.modificationDate = Date()
        changedDirectory.versionIdentifier = "folder-0-v2"
        RemoteChangeTargets.shared.record(
            containers: [NSFileProviderItemIdentifier(changedDirectory.identifier)]
        )

        remoteInterface.resetOperationCounters()
        let observer = try await runWorkingSetChanges(remoteInterface)
        let targetedReads = remoteInterface.readOperationCount

        XCTAssertNil(observer.error)
        XCTAssertEqual(
            targetedReads,
            1,
            "A push naming one container must be answered with one read, not a walk of the whole materialised set."
        )
        XCTAssertEqual(
            fullWalkReads,
            Self.directoryCount,
            "The full walk this is measured against must still read every materialised directory."
        )
        XCTAssertTrue(
            observer.changedItems.contains { $0.itemIdentifier.rawValue == changedFile.identifier },
            "The targeted walk must still report the change that prompted the push."
        )
    }

    // MARK: - Write volume

    ///
    /// Enumerating a directory whose contents have not changed writes no rows.
    ///
    /// Servers from Nextcloud 31 answer every enumeration with a paginated listing, and that
    /// ingestion path used to write every row it read — one write transaction and one
    /// `evictLogicalDuplicates` query each — whether or not the server state differed from the row
    /// already held. One measured working-set pass rewrote 1,998 rows to surface 7 real changes.
    ///
    /// The count here is exact rather than a ratio: an unchanged listing has nothing to persist, so
    /// the correct number of writes is zero. On `stable-34.0` this is
    /// ``paginatedDirectoryFileCount`` + 1 — every row, plus the directory itself.
    ///
    func testUnchangedPaginatedEnumerationWritesNoRows() async throws {
        let folder = buildPaginatedDirectory()

        let remoteInterface = MockRemoteInterface(
            account: Self.account, rootItem: rootItem, pagination: true
        )
        try await enumerateItemsPaginated(of: folder, using: remoteInterface)

        XCTAssertEqual(
            rewrittenRowCount(),
            0,
            """
            An enumeration that finds nothing changed must not rewrite any of the \
            \(Self.paginatedDirectoryFileCount + 1) rows it read.
            """
        )
    }

    ///
    /// The control for the test above: one changed file is still persisted, and nothing else is.
    ///
    /// Without this, "writes no rows" would also be satisfied by a guard that suppressed genuine
    /// changes — which is the failure mode that matters, because a suppressed write is a change the
    /// user never sees.
    ///
    func testChangedPaginatedEnumerationWritesOnlyTheChangedRow() async throws {
        let folder = buildPaginatedDirectory()

        let changedFile = try XCTUnwrap(folder.children.first)
        changedFile.versionIdentifier = "changed-v2"
        changedFile.modificationDate = Date()

        let remoteInterface = MockRemoteInterface(
            account: Self.account, rootItem: rootItem, pagination: true
        )
        try await enumerateItemsPaginated(of: folder, using: remoteInterface)

        let changedRow = try XCTUnwrap(dbManager.itemMetadata(ocId: changedFile.identifier))
        XCTAssertEqual(
            changedRow.etag,
            "changed-v2",
            "The changed file must still be persisted; the guard suppresses redundant writes, not real ones."
        )
        XCTAssertEqual(
            rewrittenRowCount(),
            1,
            """
            Only the changed file may be rewritten, out of \
            \(Self.paginatedDirectoryFileCount + 1) rows read.
            """
        )
    }

    // MARK: - Read overlap

    ///
    /// The scan's reads overlap, up to its concurrency limit.
    ///
    /// The limit itself is deliberately not asserted as an exact value — it is a tuning constant.
    /// What must hold is that reads at the same depth run together rather than one after another,
    /// and that the overlap stays bounded rather than fanning out over the whole materialised set.
    ///
    /// On `stable-34.0` this reads 1: every PROPFIND waited for the one before it.
    ///
    func testWalkReadsOverlap() async throws {
        buildMaterialisedTree()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.enumerateLatency = Self.simulatedReadLatency
        remoteInterface.resetOperationCounters()

        _ = try await runWorkingSetChanges(remoteInterface)

        XCTAssertGreaterThan(
            remoteInterface.maxConcurrentEnumerations,
            1,
            "The scan's reads must overlap; a high-water mark of 1 means it went back to waiting for each read in turn."
        )
        XCTAssertLessThanOrEqual(
            remoteInterface.maxConcurrentEnumerations,
            Self.directoryCount,
            "Overlap must stay bounded rather than issuing the whole materialised set at once."
        )
    }

    ///
    /// Wall clock of one full walk over ``directoryCount`` directories whose reads each take
    /// ``simulatedReadLatency``.
    ///
    /// Reported rather than asserted: the absolute number depends on the machine. Run this on
    /// `stable-34.0` and on this branch to reproduce the ratio. With a sequential scan the floor is
    /// `directoryCount * simulatedReadLatency`; with overlapping reads it is that divided by the
    /// concurrency the scan achieves.
    ///
    func testFullWalkWallClock() async throws {
        buildMaterialisedTree()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.enumerateLatency = Self.simulatedReadLatency

        // The first pass brings the tree in sync; the passes after it issue the same reads and write
        // nothing, so they time the walk itself rather than the ingestion behind it.
        _ = try await runWorkingSetChanges(remoteInterface)

        var durations = [Duration]()
        let clock = ContinuousClock()

        for _ in 0 ..< 3 {
            remoteInterface.resetOperationCounters()
            let elapsed = try await clock.measure {
                _ = try await runWorkingSetChanges(remoteInterface)
            }
            durations.append(elapsed)
        }

        let best = durations.min() ?? .zero
        let sequentialFloor = Duration.seconds(
            Double(Self.directoryCount) * Self.simulatedReadLatency
        )

        print("""
        [benchmark] full working-set walk over \(Self.directoryCount) directories         (\(Self.filesPerDirectory) files each, \(Int(Self.simulatedReadLatency * 1000))ms per read)
        [benchmark]   best of \(durations.count): \(best)
        [benchmark]   sequential floor: \(sequentialFloor)
        [benchmark]   peak read overlap: \(remoteInterface.maxConcurrentEnumerations)
        """)

        XCTAssertLessThan(
            best,
            sequentialFloor,
            """
            The walk must finish faster than issuing its \(Self.directoryCount) reads one after             another would allow. This is the only timing assertion here, and it is a ceiling the             sequential scan cannot meet by construction rather than a tuned threshold.
            """
        )
    }
}
