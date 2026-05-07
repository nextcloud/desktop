//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import XCTest

///
/// Tests for the recursive "Always keep downloaded" behaviour covering the
/// database-level propagation that `Item.set(keepDownloaded:domain:)` depends
/// on. The `NSFileProviderManager` side (download requests, last-used-date
/// bumps) is intentionally out of scope here — it cannot be exercised without
/// a registered File Provider extension. See issue #9057.
///
final class KeepDownloadedRecursiveTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(
        account: account,
        databaseDirectory: makeDatabaseDirectory(),
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
        log: FileProviderLogMock()
    )

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    ///
    /// Seed a small tree — folder → subfolder → file, folder → file — into
    /// the database and return the top-level folder's metadata.
    ///
    private func seedTree() throws -> SendableItemMetadata {
        let folder = RealmItemMetadata()
        folder.ocId = "folder-1"
        folder.account = "TestAccount"
        folder.serverUrl = "https://cloud.example.com/files"
        folder.fileName = "documents"
        folder.directory = true

        let directChildFile = RealmItemMetadata()
        directChildFile.ocId = "direct-child-file"
        directChildFile.account = "TestAccount"
        directChildFile.serverUrl = "https://cloud.example.com/files/documents"
        directChildFile.fileName = "report.pdf"

        let subfolder = RealmItemMetadata()
        subfolder.ocId = "subfolder-1"
        subfolder.account = "TestAccount"
        subfolder.serverUrl = "https://cloud.example.com/files/documents"
        subfolder.fileName = "nested"
        subfolder.directory = true

        let deepFile = RealmItemMetadata()
        deepFile.ocId = "deep-file"
        deepFile.account = "TestAccount"
        deepFile.serverUrl = "https://cloud.example.com/files/documents/nested"
        deepFile.fileName = "note.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(folder)
            realm.add(directChildFile)
            realm.add(subfolder)
            realm.add(deepFile)
        }

        return SendableItemMetadata(value: folder)
    }

    ///
    /// `childItems(directoryMetadata:)` must return the full subtree, not just
    /// direct children. This is the primitive the recursive keep-downloaded
    /// flow relies on — if it ever regressed to depth-1, every pinned folder
    /// would silently leak deep descendants.
    ///
    func testChildItemsReturnsFullSubtree() throws {
        let folderMetadata = try seedTree()

        let descendants = Self.dbManager.childItems(directoryMetadata: folderMetadata)

        let descendantOcIds = Set(descendants.map(\.ocId))
        XCTAssertEqual(
            descendantOcIds,
            ["direct-child-file", "subfolder-1", "deep-file"],
            "childItems must walk the whole subtree, including grandchildren."
        )
    }

    ///
    /// Apply the keep-downloaded flag to every descendant returned by
    /// `childItems`, mirroring what `Item.set(keepDownloaded:domain:)` does
    /// after its recursive enumeration. Every descendant must end up pinned.
    ///
    func testRecursivePropagationEnablesKeepDownloadedOnAllDescendants() throws {
        let folderMetadata = try seedTree()

        // Flip the root.
        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        // Flip every descendant — this is the loop Item.set performs.
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        for ocId in ["folder-1", "direct-child-file", "subfolder-1", "deep-file"] {
            let stored = try XCTUnwrap(
                Self.dbManager.itemMetadata(ocId: ocId),
                "Seeded metadata \(ocId) must still be present."
            )
            XCTAssertTrue(
                stored.keepDownloaded,
                "\(ocId) must be flagged keepDownloaded after recursive enable."
            )
        }
    }

    ///
    /// The inverse: disabling on the folder must clear the flag on every
    /// descendant, so the Finder UI and the eviction policy stop treating
    /// them as pinned.
    ///
    func testRecursivePropagationClearsKeepDownloadedOnAllDescendants() throws {
        let folderMetadata = try seedTree()

        // Seed: everything pinned first.
        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        // Act: clear.
        _ = try Self.dbManager.set(keepDownloaded: false, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: false, for: child)
        }

        for ocId in ["folder-1", "direct-child-file", "subfolder-1", "deep-file"] {
            let stored = try XCTUnwrap(
                Self.dbManager.itemMetadata(ocId: ocId),
                "Seeded metadata \(ocId) must still be present."
            )
            XCTAssertFalse(
                stored.keepDownloaded,
                "\(ocId) must no longer be flagged keepDownloaded after recursive disable."
            )
        }
    }

    ///
    /// Siblings that live outside the pinned subtree must not be touched.
    ///
    /// This guards against a `serverUrl.starts(with:)` prefix-match hazard —
    /// `"…/documents"` is a prefix of `"…/documents-archive"`, but the latter
    /// is a sibling folder and must keep its original flag state.
    ///
    ///
    /// Items flagged as "Always keep downloaded" must expose a Finder decoration
    /// Without this, the user has no visual cue that an item is pinned — which was the whole point of the feature.
    ///
    func testItemDecorationsReflectKeepDownloadedFlag() throws {
        var pinnedMetadata = SendableItemMetadata(ocId: "pinned-id", fileName: "pinned.txt", account: Self.account)
        pinnedMetadata.keepDownloaded = true

        let pinnedItem = Item(
            metadata: pinnedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        let decoration = try XCTUnwrap(pinnedItem.decorations?.first)

        XCTAssertTrue(decoration.rawValue.hasSuffix(".keep-downloaded"), "Pinned items must expose the keepDownloaded badge identifier.")

        let unpinnedMetadata = SendableItemMetadata(ocId: "unpinned-id", fileName: "unpinned.txt", account: Self.account)

        let unpinnedItem = Item(
            metadata: unpinnedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        XCTAssertNil(unpinnedItem.decorations, "Unpinned items must not declare any badge, so Finder shows no overlay.")
    }

    ///
    /// Seed `seedTree`'s structure plus an additional sibling at each level
    /// along the path to the deep file. After recursive pin and a fragmenting
    /// unpin of the deep file, the path ancestors must lose `keepDownloaded`
    /// and every off-path sibling must keep it (#9891).
    ///
    /// Tree shape:
    ///     documents/                      (path ancestor — pin root)
    ///       report.pdf                    (off-path sibling at level 0)
    ///       nested/                       (path ancestor)
    ///         note.txt                    (deep file — the unpin target)
    ///         level-1-sibling.txt         (off-path sibling at level 1)
    ///       documents-archive.zip         lives outside `documents/`; only
    ///                                     here as a control to ensure the walk
    ///                                     stops at the first non-pinned ancestor
    ///                                     even when names share a prefix.
    ///
    func testFragmentDeepUnpinUnderRecursivePin() throws {
        _ = try seedTree()

        let levelOneSibling = RealmItemMetadata()
        levelOneSibling.ocId = "level-1-sibling"
        levelOneSibling.account = "TestAccount"
        levelOneSibling.serverUrl = "https://cloud.example.com/files/documents/nested"
        levelOneSibling.fileName = "level-1-sibling.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(levelOneSibling) }

        // Seed: pin the whole subtree (mirrors the recursive enable path).
        let folderMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1"))
        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        // Act: fragment the path from deep file up to the pin root.
        let deepFileMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "deep-file"))
        let deepFileItem = Item(
            metadata: deepFileMetadata,
            parentItemIdentifier: NSFileProviderItemIdentifier("subfolder-1"),
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        let outcome = deepFileItem.fragmentPathToRootInDatabase()

        // The walk must touch both intermediate dir and root, immediate parent first.
        XCTAssertEqual(outcome.unpinnedAncestors.map(\.ocId), ["subfolder-1", "folder-1"])
        // Every cousin was already pinned by the recursive enable, so nothing
        // newly transitioned from `.inherited` to strict.
        XCTAssertTrue(outcome.newlyPinnedCousins.isEmpty)

        // Path ancestors flipped to false.
        for ocId in ["folder-1", "subfolder-1"] {
            let stored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: ocId))
            XCTAssertFalse(
                stored.keepDownloaded,
                "Path ancestor \(ocId) must be unpinned after fragmentation."
            )
        }

        // Off-path siblings retain their pin (the recursive enable already set them).
        for ocId in ["direct-child-file", "level-1-sibling"] {
            let stored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: ocId))
            XCTAssertTrue(
                stored.keepDownloaded,
                "Off-path sibling \(ocId) must remain pinned after fragmentation."
            )
        }

        // The deep file itself was not touched by fragmentation (the caller —
        // `Item.set(keepDownloaded:domain:)` — flips it before invoking the
        // fragmentation walk). At this point it is still flagged as pinned in
        // the DB, which we assert to pin down the contract: fragmentation
        // operates only on ancestors.
        let deepStored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "deep-file"))
        XCTAssertTrue(
            deepStored.keepDownloaded,
            "fragmentPathToRootInDatabase must not touch the target item."
        )
    }

    ///
    /// Cousins enumerated *after* the original recursive enable arrive in the
    /// database with `keepDownloaded == false` — the recursive walk has long
    /// since finished and there is nobody to back-fill the flag for new
    /// siblings. Without explicit cousin pinning, fragmenting an unpin path
    /// would silently drop them out of the strict-pin chain when the path
    /// ancestors flip to `.inherited` (#9891).
    ///
    /// The fragmentation walk must therefore set the flag on every off-path
    /// immediate child it finds without it, signal those cousins back via
    /// `newlyPinnedCousins`, and only then flip the path ancestor.
    ///
    func testFragmentPinsCousinsThatLackKeepDownloadedFlag() throws {
        _ = try seedTree()

        // An off-path sibling at the deepest level that was added to the DB
        // *after* the original recursive pin and so missed it.
        let lateCousin = RealmItemMetadata()
        lateCousin.ocId = "late-cousin"
        lateCousin.account = "TestAccount"
        lateCousin.serverUrl = "https://cloud.example.com/files/documents/nested"
        lateCousin.fileName = "late-cousin.txt"
        // No keepDownloaded set — defaults to false.

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(lateCousin) }

        // Original recursive enable: every then-known descendant gets flagged.
        let folderMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1"))
        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) where child.ocId != "late-cousin" {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }
        // Sanity: the late cousin still has no flag.
        XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "late-cousin")).keepDownloaded)

        // Act: fragment from the deep file.
        let deepFileMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "deep-file"))
        let deepFileItem = Item(
            metadata: deepFileMetadata,
            parentItemIdentifier: NSFileProviderItemIdentifier("subfolder-1"),
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        let outcome = deepFileItem.fragmentPathToRootInDatabase()

        // The late cousin must be reported as newly pinned and persisted with
        // the flag set, so once the path ancestors flip to `.inherited` the
        // OS still sees an explicit `.downloadEagerlyAndKeepDownloaded` here.
        XCTAssertEqual(outcome.newlyPinnedCousins.map(\.ocId), ["late-cousin"])
        XCTAssertTrue(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "late-cousin")).keepDownloaded)

        // The off-path sibling that was already pinned must not be re-reported.
        XCTAssertFalse(
            outcome.newlyPinnedCousins.map(\.ocId).contains("direct-child-file"),
            "Cousins already flagged at fragmentation time must not be reported as newly pinned."
        )
        XCTAssertTrue(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "direct-child-file")).keepDownloaded)
    }

    ///
    /// Re-pinning the original pin root after fragmentation must collapse the
    /// hole. The recursive enable in `Item.set(keepDownloaded:domain:)` does
    /// this naturally — every descendant flag is overwritten to `true`.
    ///
    func testRePinAfterFragmentationCollapses() throws {
        _ = try seedTree()

        let folderMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1"))
        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        // Fragment from the deep file.
        let deepFileMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "deep-file"))
        let deepFileItem = Item(
            metadata: deepFileMetadata,
            parentItemIdentifier: NSFileProviderItemIdentifier("subfolder-1"),
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        _ = deepFileItem.fragmentPathToRootInDatabase()
        // Plus the deep file itself, as Item.set(keepDownloaded:domain:) would.
        _ = try Self.dbManager.set(keepDownloaded: false, for: deepFileMetadata)

        // Sanity: pin root and intermediate dir are now unpinned.
        XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1")).keepDownloaded)
        XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "subfolder-1")).keepDownloaded)

        // Re-pin the root using the same pattern Item.set follows.
        let refreshedFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1"))
        _ = try Self.dbManager.set(keepDownloaded: true, for: refreshedFolder)
        for child in Self.dbManager.childItems(directoryMetadata: refreshedFolder) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        for ocId in ["folder-1", "direct-child-file", "subfolder-1", "deep-file"] {
            let stored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: ocId))
            XCTAssertTrue(
                stored.keepDownloaded,
                "\(ocId) must be pinned again after re-pinning the root."
            )
        }
    }

    ///
    /// Unpinning the pin root directly must not produce any fragmentation —
    /// there is no strict ancestor above it to cut. The walk returns an empty
    /// list and no DB writes happen on parents.
    ///
    func testShallowUnpinIsNoFragmentation() throws {
        _ = try seedTree()

        let folderMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1"))
        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        // Act: fragment from the pin root itself.
        let pinnedFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folder-1"))
        let folderItem = Item(
            metadata: pinnedFolder,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        let outcome = folderItem.fragmentPathToRootInDatabase()

        XCTAssertTrue(
            outcome.unpinnedAncestors.isEmpty,
            "Fragmenting from the pin root must touch no ancestors — there is no strict ancestor."
        )
        XCTAssertTrue(
            outcome.newlyPinnedCousins.isEmpty,
            "Fragmenting from the pin root must not touch any cousins."
        )

        // Sanity: the root and every descendant are still pinned. The fragmentation
        // walk does not touch the target item, and the recursive disable that
        // `Item.set(keepDownloaded:domain:)` performs after fragmentation is what
        // would clear them — and we have not invoked it here.
        for ocId in ["folder-1", "direct-child-file", "subfolder-1", "deep-file"] {
            let stored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: ocId))
            XCTAssertTrue(stored.keepDownloaded)
        }
    }

    func testRecursivePropagationDoesNotLeakToSiblingsWithSimilarNames() throws {
        let folderMetadata = try seedTree()

        let sibling = RealmItemMetadata()
        sibling.ocId = "sibling-file"
        sibling.account = "TestAccount"
        sibling.serverUrl = "https://cloud.example.com/files"
        sibling.fileName = "documents-archive.zip"
        // Same parent ("/files") as the target folder, but NOT inside the
        // target folder. A naive prefix match against "/files/documents"
        // would still reject this — but if anyone ever changed the match to
        // use just the parent path, this guards against that regression.

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(sibling) }

        _ = try Self.dbManager.set(keepDownloaded: true, for: folderMetadata)
        for child in Self.dbManager.childItems(directoryMetadata: folderMetadata) {
            _ = try Self.dbManager.set(keepDownloaded: true, for: child)
        }

        let storedSibling = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "sibling-file"))
        XCTAssertFalse(
            storedSibling.keepDownloaded,
            "A sibling outside the pinned folder must not inherit keepDownloaded."
        )
    }
}
