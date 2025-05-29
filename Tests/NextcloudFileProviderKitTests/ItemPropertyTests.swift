//
//  ItemPropertyTests.swift
//
//
//  Created by Claudio Cambra on 24/7/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import TestInterface
import UniformTypeIdentifiers
import XCTest
@testable import NextcloudFileProviderKit

final class ItemPropertyTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    static let dbManager = FilesDatabaseManager(
        realmConfig: .defaultConfiguration, account: account
    )

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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemD.userInfo?["displayKeepDownloaded"] as? Bool, true)
        XCTAssertEqual(itemD.userInfo?["displayAllowAutoEvicting"] as? Bool, false)
        XCTAssertEqual(itemD.userInfo?["displayEvict"] as? Bool, true)
    }

    func testItemLockFileUntrashable() {
        let metadata = SendableItemMetadata(
            ocId: "test-id", fileName: ".~lock.test.doc#", account: Self.account
        )
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        XCTAssertFalse(item.capabilities.contains(.allowsTrashing))
    }

    func testItemTrashabilityAffectedByCapabilities() async {
        let remoteInterface = MockRemoteInterface()
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
            remoteSupportsTrash: remoteSupportsTrash
        )
        XCTAssertTrue(item.capabilities.contains(.allowsTrashing))
    }

    func testStoredItemTrashabilityFalseAffectedByCapabilities() async {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let remoteInterface = MockRemoteInterface()
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
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item?.capabilities.contains(.allowsTrashing), false)
    }

    func testStoredItemTrashabilityTrueAffectedByCapabilities() async {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let remoteInterface = MockRemoteInterface()
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        let metadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test", account: Self.account)
        Self.dbManager.addItemMetadata(metadata)
        let item = await Item.storedItem(
            identifier: .init(metadata.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        XCTAssertEqual(item?.capabilities.contains(.allowsTrashing), true)
    }

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
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        XCTAssertTrue(sharedItem.isShared)
        XCTAssertTrue(sharedItem.isSharedByCurrentUser)
        XCTAssertNil(sharedItem.ownerNameComponents) // Should be nil if it is shared by us

        var sharedByOtherMetadata = sharedMetadata
        sharedByOtherMetadata.ownerId = "claucambra"
        sharedByOtherMetadata.ownerDisplayName = "Claudio Cambra"
        let sharedByOtherTime = Item(
            metadata: sharedByOtherMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        XCTAssertTrue(sharedByOtherTime.isShared)
        XCTAssertFalse(sharedByOtherTime.isSharedByCurrentUser)
        XCTAssertNotNil(sharedByOtherTime.ownerNameComponents)
        XCTAssertEqual(sharedByOtherTime.ownerNameComponents?.givenName, "Claudio")
        XCTAssertEqual(sharedByOtherTime.ownerNameComponents?.familyName, "Cambra")

        var notSharedMetadata =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        notSharedMetadata.ownerId = Self.account.id
        notSharedMetadata.ownerDisplayName = "Mr. Tester Testarino"
        let notSharedItem = Item(
            metadata: notSharedMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(),
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
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemA.contentPolicy, .downloadEagerlyAndKeepDownloaded)

        let metadataB =
            SendableItemMetadata(ocId: "test-id", fileName: "test.txt", account: Self.account)
        let itemB = Item(
            metadata: metadataB,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        XCTAssertEqual(itemB.contentPolicy, .inherited)
    }
}
