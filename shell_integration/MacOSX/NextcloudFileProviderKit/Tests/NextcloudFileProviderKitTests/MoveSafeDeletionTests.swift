//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
@testable import TestInterface
import XCTest

// MARK: - Move-safe deletion

final class MoveSafeDeletionTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(
        account: account,
        databaseDirectory: makeDatabaseDirectory(),
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
        log: FileProviderLogMock()
    )

    /// Retains the in-memory Realm for the whole test. Without a live reference the
    /// store is deallocated once a synchronous write returns, so data written before
    /// an `await` vanishes when the enumerator reopens the Realm on another thread.
    private var keepAliveRealm: Realm?

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        keepAliveRealm = Self.dbManager.ncDatabase()
    }

    override func tearDown() {
        keepAliveRealm = nil
        super.tearDown()
    }

    func testDeleteDirectorySkipsChildrenWithPendingUpload() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "upload-dir"
        dir.account = "TestAccount"
        dir.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "uploads")
        dir.directory = true

        let normalChild = RealmItemMetadata()
        normalChild.ocId = "normal-child"
        normalChild.account = "TestAccount"
        normalChild.updateLocation(serverUrl: "https://cloud.example.com/files/uploads", fileName: "synced.txt")
        normalChild.status = Status.normal.rawValue

        let uploadingChild = RealmItemMetadata()
        uploadingChild.ocId = "uploading-child"
        uploadingChild.account = "TestAccount"
        uploadingChild.updateLocation(serverUrl: "https://cloud.example.com/files/uploads", fileName: "uploading.txt")
        uploadingChild.status = Status.uploading.rawValue

        let inUploadChild = RealmItemMetadata()
        inUploadChild.ocId = "inupload-child"
        inUploadChild.account = "TestAccount"
        inUploadChild.updateLocation(serverUrl: "https://cloud.example.com/files/uploads", fileName: "queued.txt")
        inUploadChild.status = Status.inUpload.rawValue

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(dir)
            realm.add(normalChild)
            realm.add(uploadingChild)
            realm.add(inUploadChild)
        }

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(
            ocId: "upload-dir"
        )

        XCTAssertNotNil(deleted)
        let deletedOcIds = deleted?.map(\.ocId) ?? []
        XCTAssertTrue(deletedOcIds.contains("upload-dir"), "Directory itself should be deleted")
        XCTAssertTrue(deletedOcIds.contains("normal-child"), "Normal child should be deleted")
        XCTAssertFalse(
            deletedOcIds.contains("uploading-child"),
            "Child with uploading status should be skipped"
        )
        XCTAssertFalse(
            deletedOcIds.contains("inupload-child"),
            "Child with inUpload status should be skipped"
        )

        let uploadingItem = Self.dbManager.itemMetadata(ocId: "uploading-child")
        XCTAssertNotNil(uploadingItem)
        XCTAssertFalse(
            uploadingItem?.deleted ?? true,
            "Uploading child should not be marked as deleted"
        )
    }

    /// `uploadError (4)` satisfies `status >= inUpload (2)`, so items that failed
    /// to upload are preserved just like items that are actively uploading.
    /// This keeps the upload-error state visible to the user rather than losing it silently.
    func testDeleteDirectorySkipsChildrenWithUploadError() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "uperr-dir"
        dir.account = "TestAccount"
        dir.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "work")
        dir.directory = true

        let uploadErrorChild = RealmItemMetadata()
        uploadErrorChild.ocId = "uperr-child"
        uploadErrorChild.account = "TestAccount"
        uploadErrorChild.updateLocation(serverUrl: "https://cloud.example.com/files/work", fileName: "failed.txt")
        uploadErrorChild.status = Status.uploadError.rawValue

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(dir)
            realm.add(uploadErrorChild)
        }

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "uperr-dir")

        XCTAssertNotNil(deleted)
        let deletedOcIds = deleted?.map(\.ocId) ?? []
        XCTAssertFalse(
            deletedOcIds.contains("uperr-child"),
            "Child with uploadError status must be skipped — status >= inUpload protects it."
        )

        let survivingChild = Self.dbManager.itemMetadata(ocId: "uperr-child")
        XCTAssertNotNil(survivingChild)
        XCTAssertFalse(survivingChild?.deleted ?? true)
    }

    func testDeleteDirectoryDeletesChildrenWithDownloadError() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "dl-err-dir"
        dir.account = "TestAccount"
        dir.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "errors")
        dir.directory = true

        let dlErrorChild = RealmItemMetadata()
        dlErrorChild.ocId = "dl-err-child"
        dlErrorChild.account = "TestAccount"
        dlErrorChild.updateLocation(serverUrl: "https://cloud.example.com/files/errors", fileName: "broken.pdf")
        dlErrorChild.status = Status.downloadError.rawValue

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(dir)
            realm.add(dlErrorChild)
        }

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(
            ocId: "dl-err-dir"
        )

        XCTAssertNotNil(deleted)
        let deletedOcIds = deleted?.map(\.ocId) ?? []
        XCTAssertTrue(
            deletedOcIds.contains("dl-err-child"),
            "Download-error items should still be deleted (only upload-status items are protected)"
        )
    }

    func testDeleteDirectorySkipsLocalOriginLockFile() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "lock-del-dir"
        dir.account = "TestAccount"
        dir.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "work")
        dir.directory = true

        let normalChild = RealmItemMetadata()
        normalChild.ocId = "lock-del-normal"
        normalChild.account = "TestAccount"
        normalChild.updateLocation(serverUrl: "https://cloud.example.com/files/work", fileName: "report.docx")
        normalChild.status = Status.normal.rawValue

        let lockFile = RealmItemMetadata()
        lockFile.ocId = "lock-del-lockfile"
        lockFile.account = "TestAccount"
        lockFile.updateLocation(serverUrl: "https://cloud.example.com/files/work", fileName: ".~lock.report.docx#")
        lockFile.isLockFileOfLocalOrigin = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(dir)
            realm.add(normalChild)
            realm.add(lockFile)
        }

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "lock-del-dir")

        XCTAssertNotNil(deleted)
        let deletedOcIds = deleted?.map(\.ocId) ?? []
        XCTAssertTrue(deletedOcIds.contains("lock-del-dir"), "Directory itself must be deleted.")
        XCTAssertTrue(deletedOcIds.contains("lock-del-normal"), "Normal child must be deleted.")
        XCTAssertFalse(
            deletedOcIds.contains("lock-del-lockfile"),
            "Local-origin lock file must be preserved — the editor still has it open."
        )

        let survivingLock = Self.dbManager.itemMetadata(ocId: "lock-del-lockfile")
        XCTAssertNotNil(survivingLock)
        XCTAssertFalse(survivingLock?.deleted ?? true)
    }

    /// Item 404s at old path but is discovered at a new location by a separate
    /// enumeration pass. The deconfliction step must recognise the item as
    /// surviving (via allNewMetadatas) and NOT mark it deleted.
    func testDeconflictionPreventsDeleteWhenItemFoundAtNewLocation() async throws {
        let rootItem = MockRemoteItem.rootItem(account: Self.account)

        // "doc.txt" exists locally at root — it is checked before the destination folder.
        var oldMetadata = SendableItemMetadata(
            ocId: "moved-item", fileName: "doc.txt", account: Self.account
        )
        oldMetadata.serverUrl = Self.account.davFilesUrl
        oldMetadata.downloaded = true
        oldMetadata.uploaded = true
        oldMetadata.etag = "V1"
        oldMetadata.syncTime = Date()
        Self.dbManager.addItemMetadata(oldMetadata)

        // "destination" is a visited folder — longer path, checked second.
        var destFolderMeta = SendableItemMetadata(
            ocId: "destination", fileName: "destination", account: Self.account
        )
        destFolderMeta.directory = true
        destFolderMeta.visitedDirectory = true
        destFolderMeta.etag = "OLD"
        destFolderMeta.syncTime = Date()
        Self.dbManager.addItemMetadata(destFolderMeta)

        // On the server: root has only "destination", no "doc.txt".
        // "destination" contains "doc.txt" (moved there).
        let destRemote = MockRemoteItem(
            identifier: "destination",
            versionIdentifier: "V1",
            name: "destination",
            remotePath: Self.account.davFilesUrl + "/destination",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let movedDocRemote = MockRemoteItem(
            identifier: "moved-item",
            versionIdentifier: "V2",
            name: "doc.txt",
            remotePath: Self.account.davFilesUrl + "/destination/doc.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children = [destRemote]
        destRemote.parent = rootItem
        destRemote.children = [movedDocRemote]
        movedDocRemote.parent = destRemote

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let anchor = Enumerator.syncAnchor(at: Date().addingTimeInterval(-300))

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: anchor)

        let deletedIds = Set(observer.deletedItemIdentifiers.map(\.rawValue))
        XCTAssertFalse(
            deletedIds.contains("moved-item"),
            "Item found at new location must not be reported as deleted"
        )
    }

    func testParent404ReportsParentAsDeleted() async throws {
        let rootItem = MockRemoteItem.rootItem(account: Self.account)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Parent folder that no longer exists on the server.
        var parentMetadata = SendableItemMetadata(
            ocId: "parent404", fileName: "movedFolder", account: Self.account
        )
        parentMetadata.directory = true
        parentMetadata.visitedDirectory = true
        parentMetadata.syncTime = Date()
        Self.dbManager.addItemMetadata(parentMetadata)

        let anchor = Enumerator.syncAnchor(at: Date().addingTimeInterval(-300))

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: anchor)

        let parentDeleted = observer.deletedItemIdentifiers.contains {
            $0.rawValue == "parent404"
        }
        XCTAssertTrue(parentDeleted, "Folder that 404ed should be reported as deleted")
    }

    /// When a folder rename and its children are both pending in the working-set change
    /// list, macOS processes items in the order the extension reports them. If a child
    /// arrives before its parent's rename, macOS creates the destination folder to house
    /// the child — then cannot complete the parent rename because the destination already
    /// exists, leaving both the old and new folder name visible on disk simultaneously.
    ///
    /// Regression test: verify that pendingWorkingSetChanges output, when sorted by the
    /// fix applied in completeChangesObserver, places parent directories before children.
    func testPendingChangesAreSortedParentBeforeChild() throws {
        let now = Date()
        let recentSync = now.addingTimeInterval(-60)
        let anchorDate = now.addingTimeInterval(-300)

        // Parent directory and child file both have syncTime newer than the anchor so
        // they appear in pendingWorkingSetChanges.
        var dirMeta = SendableItemMetadata(
            ocId: "sort-dir", fileName: "2026-renamed", account: Self.account
        )
        dirMeta.serverUrl = Self.account.davFilesUrl + "/container"
        dirMeta.directory = true
        dirMeta.visitedDirectory = true
        dirMeta.etag = "V2"
        dirMeta.uploaded = true
        dirMeta.syncTime = recentSync
        Self.dbManager.addItemMetadata(dirMeta)

        var fileMeta = SendableItemMetadata(
            ocId: "sort-file", fileName: "Spreadsheet.xlsx", account: Self.account
        )
        fileMeta.serverUrl = Self.account.davFilesUrl + "/container/2026-renamed"
        fileMeta.downloaded = true
        fileMeta.uploaded = true
        fileMeta.etag = "V2"
        fileMeta.syncTime = recentSync
        Self.dbManager.addItemMetadata(fileMeta)

        let pending = Self.dbManager.pendingWorkingSetChanges(since: anchorDate)

        // Both items must be in the pending list.
        XCTAssertTrue(pending.updated.contains(where: { $0.ocId == "sort-dir" }))
        XCTAssertTrue(pending.updated.contains(where: { $0.ocId == "sort-file" }))

        // Apply the same sort that completeChangesObserver uses before reporting to macOS.
        let sorted = pending.updated.sorted { $0.remotePath().count < $1.remotePath().count }

        let dirIndex = try XCTUnwrap(sorted.firstIndex(where: { $0.ocId == "sort-dir" }))
        let fileIndex = try XCTUnwrap(sorted.firstIndex(where: { $0.ocId == "sort-file" }))

        XCTAssertLessThan(
            dirIndex, fileIndex,
            "Parent directory must sort before its children to prevent duplicate folders on rename."
        )
    }
}
