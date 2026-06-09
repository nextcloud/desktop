//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
@testable import TestInterface
import XCTest

// MARK: - Lock token invalidation

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
