//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import UniformTypeIdentifiers
import XCTest

final class ItemPropertyTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    func testMetadataContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadata.etag = "test-etag"
        metadata.contentType = UTType.text.identifier
        metadata.size = 12

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item.contentType, UTType.text)
    }

    func testMetadataExtensionContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.pdf", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        // Don't set the content type in metadata, test the extension uttype discovery

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item.contentType, UTType.pdf)
    }

    func testMetadataFolderContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        metadata.directory = true

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item.contentType, UTType.folder)
    }

    func testMetadataPackageContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.zip", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = UTType.package.identifier

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item.contentType, UTType.package)
    }

    func testMetadataBundleContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.key", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = UTType.bundle.identifier

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item.contentType, UTType.bundle)
    }

    func testMetadataUnixFolderContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = "httpd/unix-directory"

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item.contentType, UTType.folder)
    }

    func testPredictedBundleContentType() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.app", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = "httpd/unix-directory"

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertTrue(item.contentType.conforms(to: .bundle))
    }

    func testItemUserInfoLockingPropsFileLocked() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadata.etag = "test-etag"
        metadata.size = 12
        metadata.lock = true
        metadata.lockOwner = Self.account.username
        metadata.lockOwnerEditor = "testEditor"
        metadata.lockTime = .init()
        metadata.lockTimeOut = .init().addingTimeInterval(6_000_000)

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        XCTAssertNotNil(item.userInfo?["locked"])

        let fileproviderItems = ["fileproviderItems": [item]]
        let lockPredicate = NSPredicate(
            format: "SUBQUERY ( fileproviderItems, $fileproviderItem, $fileproviderItem.userInfo.locked != nil ).@count > 0"
        )
        XCTAssertTrue(lockPredicate.evaluate(with: fileproviderItems))
    }

    func testItemUserInfoLockingPropsFileUnlocked() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadata.etag = "test-etag"
        metadata.date = .init()
        metadata.size = 12

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        XCTAssertNil(item.userInfo?["locked"])

        let fileproviderItems = ["fileproviderItems": [item]]
        let lockPredicate = NSPredicate(
            format: "SUBQUERY ( fileproviderItems, $fileproviderItem, $fileproviderItem.userInfo.locked == nil ).@count > 0"
        )
        XCTAssertTrue(lockPredicate.evaluate(with: fileproviderItems))
    }

    func testItemUserInfoDisplayEvictState() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadata.downloaded = true

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        XCTAssertNotNil(item.userInfo?["displayEvict"])

        let fileproviderItems = ["fileproviderItems": [item]]
        let canEvictPredicate = NSPredicate(
            format: "SUBQUERY ( fileproviderItems, $fileproviderItem, $fileproviderItem.userInfo.displayEvict == true ).@count > 0"
        )
        XCTAssertTrue(canEvictPredicate.evaluate(with: fileproviderItems))

        metadata.keepDownloaded = true
        let keepDownloadedItem = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertNotNil(keepDownloadedItem.userInfo?["displayEvict"])

        let fileproviderKeepDownloadedItems = ["fileproviderItems": [keepDownloadedItem]]
        let cannotEvictPredicate = NSPredicate(
            format: "SUBQUERY ( fileproviderItems, $fileproviderItem, $fileproviderItem.userInfo.displayEvict == true ).@count > 0"
        )
        XCTAssertFalse(cannotEvictPredicate.evaluate(with: fileproviderKeepDownloadedItems))
    }

    func testItemUserInfoNoDisplayEvictState() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadata.downloaded = false

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        XCTAssertNotNil(item.userInfo?["displayEvict"])

        let fileproviderItems = ["fileproviderItems": [item]]
        let undownloadedPredicate = NSPredicate(
            format: "SUBQUERY ( fileproviderItems, $fileproviderItem, $fileproviderItem.userInfo.displayEvict == false ).@count > 0"
        )
        XCTAssertTrue(undownloadedPredicate.evaluate(with: fileproviderItems))
    }

    func testItemUserInfoKeepDownloadedProperties() {
        var metadataA =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadataA.keepDownloaded = true

        let itemA = Item(
            metadata: metadataA,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemA.userInfo?["displayKeepDownloaded"] as? Bool, false)
        XCTAssertEqual(itemA.userInfo?["displayAllowAutoEvicting"] as? Bool, true)
        XCTAssertEqual(itemA.userInfo?["displayEvict"] as? Bool, false)

        let metadataB =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        let itemB = Item(
            metadata: metadataB,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertTrue(itemB.userInfo?["displayKeepDownloaded"] as? Bool == true)
        XCTAssertTrue(itemB.userInfo?["displayAllowAutoEvicting"] as? Bool == false)
        XCTAssertEqual(itemB.userInfo?["displayEvict"] as? Bool, false)

        var metadataC =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadataC.keepDownloaded = true
        metadataC.downloaded = true

        let itemC = Item(
            metadata: metadataC,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemC.userInfo?["displayKeepDownloaded"] as? Bool, false)
        XCTAssertEqual(itemC.userInfo?["displayAllowAutoEvicting"] as? Bool, true)
        XCTAssertEqual(itemC.userInfo?["displayEvict"] as? Bool, false)

        var metadataD =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadataD.downloaded = true

        let itemD = Item(
            metadata: metadataD,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemD.userInfo?["displayKeepDownloaded"] as? Bool, true)
        XCTAssertEqual(itemD.userInfo?["displayAllowAutoEvicting"] as? Bool, false)
        XCTAssertEqual(itemD.userInfo?["displayEvict"] as? Bool, true)
    }

    func testItemUserInfoDisplayShare() {
        var metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadata.permissions = "GDNVW" // No "R" for shareable

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )

        XCTAssertNil(item.userInfo?["displayShare"])

        let fileproviderItems = ["fileproviderItems": [item]]
        let lockPredicate = NSPredicate(
            format: "SUBQUERY ( fileproviderItems, $fileproviderItem, $fileproviderItem.userInfo.displayShare == nil ).@count > 0"
        )
        XCTAssertTrue(lockPredicate.evaluate(with: fileproviderItems))
    }

    func testItemLockFileUntrashable() {
        let metadata = SendableItemMetadata(
            ocId: "test-id", fileName: ".~lock.test.doc#", account: Self.account
        )
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertFalse(item.capabilities.contains(.allowsTrashing))
    }

    func testItemTrashabilityAffectedByCapabilities() async {
        let remoteInterface = MockRemoteInterface(account: Self.account)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        let remoteSupportsTrash = await remoteInterface.supportsTrash(account: Self.account)
        let metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test", account: Self.account)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            remoteSupportsTrash: remoteSupportsTrash,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(item.capabilities.contains(.allowsTrashing))
    }

    func testStoredItemTrashabilityFalseAffectedByCapabilities() async {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let remoteInterface = MockRemoteInterface(account: Self.account)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")
        let metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test", account: Self.account)
        Self.dbManager.addItemMetadata(metadata)
        let item = await Item.storedItem(
            identifier: .init(metadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        XCTAssertEqual(item?.capabilities.contains(.allowsTrashing), false)
    }

    func testStoredItemTrashabilityTrueAffectedByCapabilities() async {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let remoteInterface = MockRemoteInterface(account: Self.account)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        let metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test", account: Self.account)
        Self.dbManager.addItemMetadata(metadata)
        let item = await Item.storedItem(
            identifier: .init(metadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        XCTAssertEqual(item?.capabilities.contains(.allowsTrashing), true)
    }

    func testCapabilitiesReadingFile() {
        // 1. Setup metadata for a readable file
        var metadata = SendableItemMetadata(
            ocId: "reading-file-id", fileName: "readable.txt", account: Self.account
        )
        metadata.permissions = "G"
        metadata.directory = false

        // 2. Create the item
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )

        // 3. Assertions
        XCTAssertTrue(
            item.capabilities.contains(.allowsReading),
            "Item with 'G' permission should be readable."
        )
    }

    func testCapabilitiesEnumeratingDirectory() {
        // 1. Setup metadata for a readable directory
        var metadata = SendableItemMetadata(
            ocId: "enum-dir-id", fileName: "MyFolder", account: Self.account
        )
        metadata.permissions = "G"
        metadata.directory = true

        // 2. Create the item
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )

        // 3. Assertions
        XCTAssertTrue(
            item.capabilities.contains(.allowsContentEnumerating),
            "Directory with 'G' permission should allow enumerating."
        )
    }

    func testCapabilitiesDeleting() {
        // Case 1: Deletable
        var deletableMetadata = SendableItemMetadata(
            ocId: "deletable-id", fileName: "deletable.txt", account: Self.account
        )
        deletableMetadata.permissions = "D"
        let canDeleteItem = Item(
            metadata: deletableMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(
            canDeleteItem.capabilities.contains(.allowsDeleting),
            "Item with 'D' permission should be deletable."
        )

        // Case 2: Locked
        var lockedMetadata = SendableItemMetadata(
            ocId: "locked-deletable-id", fileName: "locked.txt", account: Self.account
        )
        lockedMetadata.permissions = "D"
        lockedMetadata.lock = true
        let cannotDeleteLockedItem = Item(
            metadata: lockedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotDeleteLockedItem.capabilities.contains(.allowsDeleting),
            "Locked item should not be deletable."
        )

        // Case 3: No permission
        var noPermsMetadata = SendableItemMetadata(
            ocId: "no-del-perm-id", fileName: "readonly.txt", account: Self.account
        )
        noPermsMetadata.permissions = "G"
        let cannotDeleteNoPermsItem = Item(
            metadata: noPermsMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotDeleteNoPermsItem.capabilities.contains(.allowsDeleting),
            "Item without 'D' permission should not be deletable."
        )
    }

    func testCapabilitiesTrashing() {
        // Case 1: Can be trashed
        let trashableMetadata = SendableItemMetadata(
            ocId: "trashable-id", fileName: "trashable.txt", account: Self.account
        )
        let canTrashItem = Item(
            metadata: trashableMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(
            canTrashItem.capabilities.contains(.allowsTrashing),
            "Item should be trashable if server supports it."
        )

        // Case 2: Server does not support trash
        let cannotTrashItem = Item(
            metadata: trashableMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: false,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotTrashItem.capabilities.contains(.allowsTrashing),
            "Item should not be trashable if server does not support it."
        )

        // Case 3: Item is locked
        var lockedMetadata = SendableItemMetadata(
            ocId: "locked-trash-id", fileName: "locked.txt", account: Self.account
        )
        lockedMetadata.lock = true
        let cannotTrashLockedItem = Item(
            metadata: lockedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotTrashLockedItem.capabilities.contains(.allowsTrashing),
            "Locked item should not be trashable."
        )

        // Case 4: Item is a lock file
        let lockFileMetadata = SendableItemMetadata(
            ocId: "lockfile-id", fileName: ".~lock.file.docx#", account: Self.account
        )
        let cannotTrashLockFileItem = Item(
            metadata: lockFileMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotTrashLockFileItem.capabilities.contains(.allowsTrashing),
            "Office lock files should not be trashable."
        )
    }

    func testCapabilitiesWriting() {
        // Case 1: Writable
        var writableMetadata = SendableItemMetadata(
            ocId: "writable-id", fileName: "writable.txt", account: Self.account
        )
        writableMetadata.permissions = "W"
        let canWriteItem = Item(
            metadata: writableMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(
            canWriteItem.capabilities.contains(.allowsWriting),
            "File with 'W' permission should be writable."
        )

        // Case 2: Locked
        var lockedMetadata = writableMetadata
        lockedMetadata.ocId = "locked-writable-id"
        lockedMetadata.lock = true
        let cannotWriteLockedItem = Item(
            metadata: lockedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotWriteLockedItem.capabilities.contains(.allowsWriting),
            "Locked file should not be writable."
        )

        // Case 3: Is a directory
        var directoryMetadata = writableMetadata
        directoryMetadata.ocId = "dir-writable-id"
        directoryMetadata.directory = true
        let cannotWriteDirectoryItem = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotWriteDirectoryItem.capabilities.contains(.allowsWriting),
            "Directory should not be writable."
        )
    }

    func testCapabilitiesRenamingAndReparenting() {
        let expected: NSFileProviderItemCapabilities = [.allowsRenaming, .allowsReparenting]

        // Case 1: Can be modified
        var modifiableMetadata = SendableItemMetadata(
            ocId: "modifiable-id", fileName: "moveme.txt", account: Self.account
        )
        modifiableMetadata.permissions = "NV"
        let canModifyItem = Item(
            metadata: modifiableMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(
            canModifyItem.capabilities.isSuperset(of: expected),
            "Item with 'NV' permission should be renamable and reparentable."
        )

        // Case 2: Is locked
        var lockedMetadata = modifiableMetadata
        lockedMetadata.ocId = "locked-modifiable-id"
        lockedMetadata.lock = true
        let cannotModifyLockedItem = Item(
            metadata: lockedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotModifyLockedItem.capabilities.isSuperset(of: expected),
            "Locked item should not be renamable or reparentable."
        )
    }

    func testCapabilitiesAddingSubItems() {
        // Case 1: Can add sub-items to a directory
        var dirMetadata =
            SendableItemMetadata(ocId: "dir-add-id", fileName: "MyFolder", account: Self.account)
        dirMetadata.permissions = "NV"
        dirMetadata.directory = true
        let canAddSubItemsItem = Item(
            metadata: dirMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(
            canAddSubItemsItem.capabilities.contains(.allowsAddingSubItems),
            "Directory with 'NV' should allow adding sub-items."
        )

        // Case 2: Cannot add sub-items to a file
        var fileMetadata = dirMetadata
        fileMetadata.ocId = "file-add-id"
        fileMetadata.directory = false
        let cannotAddToFileItem = Item(
            metadata: fileMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotAddToFileItem.capabilities.contains(.allowsAddingSubItems),
            "File should not allow adding sub-items."
        )

        // Case 3: Cannot add sub-items to a locked directory
        var lockedDirMetadata = dirMetadata
        lockedDirMetadata.ocId = "locked-dir-add-id"
        lockedDirMetadata.lock = true
        let cannotAddToLockedDirItem = Item(
            metadata: lockedDirMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )
        XCTAssertFalse(
            cannotAddToLockedDirItem.capabilities.contains(.allowsAddingSubItems),
            "Locked directory should not allow adding sub-items."
        )
    }

    func testCapabilitiesFullPermissionsFile() {
        var metadata = SendableItemMetadata(
            ocId: "full-perms-file", fileName: "do-it-all.txt", account: Self.account
        )
        metadata.permissions = "RGDNVW"
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )

        let expected: NSFileProviderItemCapabilities = [
            .allowsReading,
            .allowsDeleting,
            .allowsTrashing,
            .allowsWriting,
            .allowsRenaming,
            .allowsReparenting
        ]

        // Excluding from sync is macOS-specific and always added if available
        var platformExpected = expected
        platformExpected.insert(.allowsExcludingFromSync)

        XCTAssertEqual(item.capabilities, platformExpected)
    }

    func testCapabilitiesFullPermissionsFolder() {
        var metadata = SendableItemMetadata(
            ocId: "full-perms-folder", fileName: "SuperFolder", account: Self.account
        )
        metadata.permissions = "RGDNVW"
        metadata.directory = true
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )

        let expected: NSFileProviderItemCapabilities = [
            .allowsContentEnumerating,
            .allowsDeleting,
            .allowsTrashing,
            .allowsRenaming,
            .allowsReparenting,
            .allowsAddingSubItems
        ]

        var platformExpected = expected
        platformExpected.insert(.allowsExcludingFromSync)

        XCTAssertEqual(item.capabilities, platformExpected)
    }

    func testCapabilitiesNoPermissions() {
        var metadata =
            SendableItemMetadata(ocId: "no-perms", fileName: "nothing.txt", account: Self.account)
        metadata.permissions = "" // No permissions from server
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            remoteSupportsTrash: true,
            log: FileProviderLogMock()
        )

        // Trashing and Excluding from Sync might still be allowed as they don't depend on the
        // permission string
        var expected: NSFileProviderItemCapabilities = [.allowsTrashing]
        expected.insert(.allowsExcludingFromSync)

        XCTAssertEqual(item.capabilities, expected)
    }

    #if os(macOS)
        func testCapabilitiesMacOSExclusion() {
            var metadata = SendableItemMetadata(
                ocId: "macos-exclusion", fileName: "file.txt", account: Self.account
            )
            metadata.permissions = ""
            let item = Item(
                metadata: metadata,
                parentItemIdentifier: .rootContainer,
                account: Self.account,
                remoteInterface: MockRemoteInterface(account: Self.account),
                dbManager: Self.dbManager,
                remoteSupportsTrash: true,
                log: FileProviderLogMock()
            )

            XCTAssertTrue(
                item.capabilities.contains(.allowsExcludingFromSync),
                "Should allow excluding from sync on supported macOS versions."
            )
        }
    #endif

    func testItemShared() {
        var sharedMetadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        sharedMetadata.shareType = [ShareType.publicLink.rawValue]
        sharedMetadata.ownerId = Self.account.id
        sharedMetadata.ownerDisplayName = "Mr. Tester Testarino"
        let sharedItem = Item(
            metadata: sharedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertFalse(sharedItem.isShared)
        XCTAssertFalse(sharedItem.isSharedByCurrentUser)
        XCTAssertNil(sharedItem.ownerNameComponents) // Should be nil if it is shared by us

        var sharedByOtherMetadata = sharedMetadata
        sharedByOtherMetadata.ownerId = "claucambra"
        sharedByOtherMetadata.ownerDisplayName = "Claudio Cambra"
        let sharedByOtherTime = Item(
            metadata: sharedByOtherMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertFalse(sharedByOtherTime.isShared)
        XCTAssertFalse(sharedByOtherTime.isSharedByCurrentUser)
        XCTAssertNil(sharedByOtherTime.ownerNameComponents)

        var notSharedMetadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        notSharedMetadata.ownerId = Self.account.id
        notSharedMetadata.ownerDisplayName = "Mr. Tester Testarino"
        let notSharedItem = Item(
            metadata: notSharedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        debugPrint(notSharedMetadata.shareType)
        XCTAssertFalse(notSharedItem.isShared)
        XCTAssertFalse(notSharedItem.isSharedByCurrentUser)
        XCTAssertNil(notSharedItem.ownerNameComponents)
    }

    func testContentPolicy() {
        var metadataA =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        metadataA.keepDownloaded = true

        let itemA = Item(
            metadata: metadataA,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemA.contentPolicy, .downloadEagerlyAndKeepDownloaded)

        let metadataB =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        let itemB = Item(
            metadata: metadataB,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemB.contentPolicy, .inherited)
    }
}
