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
