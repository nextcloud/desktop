//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
@testable import TestInterface
import XCTest

// MARK: - Path boundary prefix matching

final class PathBoundaryPrefixTests: NextcloudFileProviderKitTestCase {
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

    func testChildItemsMatchesDirectChildButNotSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-A"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "photos"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/photos"
        childMetadata.fileName = "pic.jpg"

        let nestedChild = RealmItemMetadata()
        nestedChild.ocId = "nested-1"
        nestedChild.account = "TestAccount"
        nestedChild.serverUrl = "https://cloud.example.com/files/photos/vacation"
        nestedChild.fileName = "beach.jpg"

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-1"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/photos-backup"
        siblingMetadata.fileName = "old.jpg"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(nestedChild)
            realm.add(siblingMetadata)
        }

        let children = Self.dbManager.childItems(
            directoryMetadata: SendableItemMetadata(value: directoryMetadata)
        )
        let childOcIds = Set(children.map(\.ocId))
        XCTAssertTrue(childOcIds.contains("child-1"), "Direct child should be matched")
        XCTAssertTrue(childOcIds.contains("nested-1"), "Nested descendant should be matched")
        XCTAssertFalse(childOcIds.contains("sibling-1"), "Sibling with shared prefix must not match")
        XCTAssertEqual(children.count, 2)
    }

    func testChildItemCountExcludesSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-B"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "docs"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-2"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/docs"
        childMetadata.fileName = "report.pdf"

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-2"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/docs-archive"
        siblingMetadata.fileName = "old-report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(siblingMetadata)
        }

        let count = Self.dbManager.childItemCount(
            directoryMetadata: SendableItemMetadata(value: directoryMetadata)
        )
        XCTAssertEqual(count, 1)
    }

    func testDeleteDirectoryDoesNotDeleteSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-C"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "work"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-3"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/work"
        childMetadata.fileName = "task.txt"

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-3"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/work-old"
        siblingMetadata.fileName = "task-old.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(siblingMetadata)
        }

        let deleted = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "dir-C")
        XCTAssertNotNil(deleted)
        XCTAssertEqual(deleted?.count, 2, "Should delete directory + direct child")

        let survivingSibling = Self.dbManager.itemMetadata(ocId: "sibling-3")
        XCTAssertNotNil(survivingSibling)
        XCTAssertFalse(survivingSibling?.deleted ?? true)
    }

    func testRenameDirectoryDoesNotRenameSiblingWithSharedPrefix() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-D"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "alpha"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "child-4"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/alpha"
        childMetadata.fileName = "file.txt"

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "sibling-4"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/alphabet"
        siblingMetadata.fileName = "a.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
            realm.add(siblingMetadata)
        }

        let updated = Self.dbManager.renameDirectoryAndPropagateToChildren(
            ocId: "dir-D",
            newServerUrl: "https://cloud.example.com/files",
            newFileName: "beta"
        )

        XCTAssertNotNil(updated)
        XCTAssertTrue(
            updated?.contains(where: { $0.ocId == "child-4" }) ?? false,
            "Direct child should be in the updated results"
        )

        let renamedChild = Self.dbManager.itemMetadata(ocId: "child-4")
        XCTAssertEqual(
            renamedChild?.serverUrl,
            "https://cloud.example.com/files/beta",
            "Direct child's serverUrl should be updated"
        )

        let sibling = Self.dbManager.itemMetadata(ocId: "sibling-4")
        XCTAssertEqual(
            sibling?.serverUrl,
            "https://cloud.example.com/files/alphabet",
            "Sibling with shared prefix should not have its serverUrl changed"
        )
    }

    func testItemMetadatasUnderServerUrlExcludesSiblingPrefix() throws {
        let directChild = RealmItemMetadata()
        directChild.ocId = "under-1"
        directChild.account = "TestAccount"
        directChild.serverUrl = "https://cloud.example.com/files/project"
        directChild.fileName = "readme.md"

        let nestedChild = RealmItemMetadata()
        nestedChild.ocId = "under-2"
        nestedChild.account = "TestAccount"
        nestedChild.serverUrl = "https://cloud.example.com/files/project/src"
        nestedChild.fileName = "main.swift"

        let siblingMetadata = RealmItemMetadata()
        siblingMetadata.ocId = "under-3"
        siblingMetadata.account = "TestAccount"
        siblingMetadata.serverUrl = "https://cloud.example.com/files/project-v2"
        siblingMetadata.fileName = "readme.md"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directChild)
            realm.add(nestedChild)
            realm.add(siblingMetadata)
        }

        let results = Self.dbManager.itemMetadatas(
            account: "TestAccount",
            underServerUrl: "https://cloud.example.com/files/project"
        )
        let resultOcIds = Set(results.map(\.ocId))
        XCTAssertTrue(resultOcIds.contains("under-1"), "Direct child should be included")
        XCTAssertTrue(resultOcIds.contains("under-2"), "Nested child should be included")
        XCTAssertFalse(resultOcIds.contains("under-3"), "Sibling with shared prefix must not match")
        XCTAssertEqual(results.count, 2)
    }
}

// MARK: - Lock token invalidation (Fix 2)

final class LockTokenInvalidationTests: NextcloudFileProviderKitTestCase {
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

    func testRenameItemMetadataClearsLockToken() throws {
        let metadata = RealmItemMetadata()
        metadata.ocId = "lock-1"
        metadata.account = "TestAccount"
        metadata.serverUrl = "https://cloud.example.com/files/old"
        metadata.fileName = "doc.txt"
        metadata.lockToken = "opaquelocktoken:abc123"

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(metadata) }

        Self.dbManager.renameItemMetadata(
            ocId: "lock-1",
            newServerUrl: "https://cloud.example.com/files/new",
            newFileName: "doc.txt"
        )

        let updated = Self.dbManager.itemMetadata(ocId: "lock-1")
        XCTAssertNotNil(updated)
        XCTAssertNil(updated?.lockToken, "Lock token must be cleared after rename")
    }

    func testRenameDirectoryClearsLockTokenOnChildren() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "lock-dir"
        dir.account = "TestAccount"
        dir.serverUrl = "https://cloud.example.com/files"
        dir.fileName = "folder"
        dir.directory = true

        let child = RealmItemMetadata()
        child.ocId = "lock-child"
        child.account = "TestAccount"
        child.serverUrl = "https://cloud.example.com/files/folder"
        child.fileName = "important.docx"
        child.lockToken = "opaquelocktoken:xyz789"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(dir)
            realm.add(child)
        }

        _ = Self.dbManager.renameDirectoryAndPropagateToChildren(
            ocId: "lock-dir",
            newServerUrl: "https://cloud.example.com/files",
            newFileName: "renamed-folder"
        )

        let updatedChild = Self.dbManager.itemMetadata(ocId: "lock-child")
        XCTAssertNotNil(updatedChild)
        XCTAssertNil(
            updatedChild?.lockToken,
            "Lock token on child must be cleared when parent directory is renamed"
        )
        XCTAssertEqual(
            updatedChild?.serverUrl,
            "https://cloud.example.com/files/renamed-folder"
        )
    }

    func testProcessUpdateClearsLockTokenWhenFileNameChanges() {
        let account = Account(
            user: "lockClear", id: "lc", serverUrl: "https://example.com", password: ""
        )

        var parentDir = SendableItemMetadata(
            ocId: "lock-clear-parent", fileName: "parentDir", account: account
        )
        parentDir.serverUrl = account.davFilesUrl
        parentDir.directory = true
        parentDir.uploaded = true
        parentDir.etag = "dir-etag-1"
        Self.dbManager.addItemMetadata(parentDir)

        var original = SendableItemMetadata(
            ocId: "lock-update", fileName: "old-name.txt", account: account
        )
        original.serverUrl = account.davFilesUrl + "/parentDir"
        original.lockToken = "opaquelocktoken:should-be-cleared"
        original.uploaded = true
        original.downloaded = true
        original.etag = "etag-1"
        Self.dbManager.addItemMetadata(original)

        var updatedParent = parentDir
        updatedParent.etag = "dir-etag-2"

        var renamed = original
        renamed.fileName = "new-name.txt"
        renamed.fileNameView = "new-name.txt"
        renamed.etag = "etag-2"

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl + "/parentDir",
            updatedMetadatas: [updatedParent, renamed],
            keepExistingDownloadState: true
        )

        let renamedResult = result.updatedMetadatas?.first(where: { $0.ocId == "lock-update" })
        XCTAssertNotNil(renamedResult, "Renamed item should appear in updated metadatas")
        XCTAssertNil(
            renamedResult?.lockToken,
            "Lock token must be cleared when file name changes during update"
        )
    }

    func testProcessUpdatePreservesLockTokenWhenPathUnchanged() {
        let account = Account(
            user: "lockKeep", id: "lk", serverUrl: "https://example.com", password: ""
        )

        var parentDir = SendableItemMetadata(
            ocId: "lock-keep-parent", fileName: "parentDir", account: account
        )
        parentDir.serverUrl = account.davFilesUrl
        parentDir.directory = true
        parentDir.uploaded = true
        parentDir.etag = "dir-etag-1"
        Self.dbManager.addItemMetadata(parentDir)

        var original = SendableItemMetadata(
            ocId: "lock-preserve", fileName: "file.txt", account: account
        )
        original.serverUrl = account.davFilesUrl + "/parentDir"
        original.lockToken = "opaquelocktoken:keep-me"
        original.uploaded = true
        original.downloaded = true
        original.etag = "etag-1"
        Self.dbManager.addItemMetadata(original)

        var updatedParent = parentDir
        updatedParent.etag = "dir-etag-2"

        var updated = original
        updated.etag = "etag-2"

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl + "/parentDir",
            updatedMetadatas: [updatedParent, updated],
            keepExistingDownloadState: true
        )

        let preservedResult = result.updatedMetadatas?.first(where: { $0.ocId == "lock-preserve" })
        XCTAssertNotNil(preservedResult, "Unchanged-path item should appear in updated metadatas")
        XCTAssertEqual(
            preservedResult?.lockToken,
            "opaquelocktoken:keep-me",
            "Lock token must be preserved when path has not changed"
        )
    }
}

// MARK: - NKError extensions (Fix 3)

final class NKErrorExtensionsTests: XCTestCase {
    func testPreconditionFailedError() {
        let error = NKError(errorCode: 412, errorDescription: "Precondition Failed")
        XCTAssertTrue(error.isPreconditionFailedError)
        XCTAssertFalse(error.isLockedError)
    }

    func testLockedError() {
        let error = NKError(errorCode: 423, errorDescription: "Locked")
        XCTAssertTrue(error.isLockedError)
        XCTAssertFalse(error.isPreconditionFailedError)
    }

    func testFileProviderErrorMapping() {
        let success = NKError.success
        XCTAssertNil(success.fileProviderError)

        let notFound = NKError(errorCode: 404, errorDescription: "")
        XCTAssertEqual(notFound.fileProviderError?.code, .noSuchItem)

        let unauthorized = NKError(errorCode: 401, errorDescription: "")
        XCTAssertEqual(unauthorized.fileProviderError?.code, .notAuthenticated)

        let quota = NKError(errorCode: 507, errorDescription: "")
        XCTAssertEqual(quota.fileProviderError?.code, .insufficientQuota)

        let collision = NKError(errorCode: 405, errorDescription: "")
        XCTAssertEqual(collision.fileProviderError?.code, .filenameCollision)

        let precondition = NKError(errorCode: 412, errorDescription: "")
        XCTAssertEqual(
            precondition.fileProviderError?.code,
            .cannotSynchronize,
            "412 maps to generic cannotSynchronize, not a special branch"
        )

        let locked = NKError(errorCode: 423, errorDescription: "")
        XCTAssertEqual(
            locked.fileProviderError?.code,
            .cannotSynchronize,
            "423 maps to generic cannotSynchronize, not a special branch"
        )
    }
}

// MARK: - Move-safe deletion (Fix 4 + 5)

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

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    func testDeleteDirectorySkipsChildrenWithPendingUpload() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "upload-dir"
        dir.account = "TestAccount"
        dir.serverUrl = "https://cloud.example.com/files"
        dir.fileName = "uploads"
        dir.directory = true

        let normalChild = RealmItemMetadata()
        normalChild.ocId = "normal-child"
        normalChild.account = "TestAccount"
        normalChild.serverUrl = "https://cloud.example.com/files/uploads"
        normalChild.fileName = "synced.txt"
        normalChild.status = Status.normal.rawValue

        let uploadingChild = RealmItemMetadata()
        uploadingChild.ocId = "uploading-child"
        uploadingChild.account = "TestAccount"
        uploadingChild.serverUrl = "https://cloud.example.com/files/uploads"
        uploadingChild.fileName = "uploading.txt"
        uploadingChild.status = Status.uploading.rawValue

        let inUploadChild = RealmItemMetadata()
        inUploadChild.ocId = "inupload-child"
        inUploadChild.account = "TestAccount"
        inUploadChild.serverUrl = "https://cloud.example.com/files/uploads"
        inUploadChild.fileName = "queued.txt"
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

    func testDeleteDirectoryDeletesChildrenWithDownloadError() throws {
        let dir = RealmItemMetadata()
        dir.ocId = "dl-err-dir"
        dir.account = "TestAccount"
        dir.serverUrl = "https://cloud.example.com/files"
        dir.fileName = "errors"
        dir.directory = true

        let dlErrorChild = RealmItemMetadata()
        dlErrorChild.ocId = "dl-err-child"
        dlErrorChild.account = "TestAccount"
        dlErrorChild.serverUrl = "https://cloud.example.com/files/errors"
        dlErrorChild.fileName = "broken.pdf"
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

        let anchorDate = Date().addingTimeInterval(-300)
        let formatter = ISO8601DateFormatter()
        let anchor = try NSFileProviderSyncAnchor(
            XCTUnwrap(formatter.string(from: anchorDate).data(using: .utf8))
        )

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

        let anchorDate = Date().addingTimeInterval(-300)
        let formatter = ISO8601DateFormatter()
        let anchor = try NSFileProviderSyncAnchor(
            XCTUnwrap(formatter.string(from: anchorDate).data(using: .utf8))
        )

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
}
