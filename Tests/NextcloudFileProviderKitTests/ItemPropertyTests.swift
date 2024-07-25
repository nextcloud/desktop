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
        user: "testUser", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    func testMetadataContentType() {
        let metadata = ItemMetadata()
        metadata.ocId = "test-id"
        metadata.etag = "test-etag"
        metadata.name = "test.txt"
        metadata.fileName = "test.txt"
        metadata.fileNameView = "test.txt"
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.urlBase = Self.account.serverUrl
        metadata.userId = Self.account.username
        metadata.user = Self.account.username
        metadata.date = .init()
        metadata.contentType = UTType.text.identifier
        metadata.size = 12

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: MockRemoteInterface(account: Self.account)
        )
        XCTAssertEqual(item.contentType, UTType.text)
    }

    func testMetadataExtensionContentType() {
        let metadata = ItemMetadata()
        metadata.ocId = "test-id"
        metadata.etag = "test-etag"
        metadata.name = "test.pdf"
        metadata.fileName = "test.pdf"
        metadata.fileNameView = "test.pdf"
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.urlBase = Self.account.serverUrl
        metadata.userId = Self.account.username
        metadata.user = Self.account.username
        metadata.date = .init()
        metadata.size = 12
        // Don't set the content type in metadata, test the extension uttype discovery

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: MockRemoteInterface(account: Self.account)
        )
        XCTAssertEqual(item.contentType, UTType.pdf)
    }

    func testMetadataFolderContentType() {
        let metadata = ItemMetadata()
        metadata.ocId = "test-id"
        metadata.etag = "test-etag"
        metadata.name = "test"
        metadata.fileName = "test"
        metadata.fileNameView = "test"
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.urlBase = Self.account.serverUrl
        metadata.userId = Self.account.username
        metadata.user = Self.account.username
        metadata.date = .init()
        metadata.size = 12
        metadata.directory = true

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: MockRemoteInterface(account: Self.account)
        )
        XCTAssertEqual(item.contentType, UTType.folder)
    }

    func testMetadataPackageContentType() {
        let metadata = ItemMetadata()
        metadata.ocId = "test-id"
        metadata.etag = "test-etag"
        metadata.name = "test.zip"
        metadata.fileName = "test.zip"
        metadata.fileNameView = "test.zip"
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.urlBase = Self.account.serverUrl
        metadata.userId = Self.account.username
        metadata.user = Self.account.username
        metadata.date = .init()
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = UTType.package.identifier

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: MockRemoteInterface(account: Self.account)
        )
        XCTAssertEqual(item.contentType, UTType.package)
    }

    func testMetadataBundleContentType() {
        let metadata = ItemMetadata()
        metadata.ocId = "test-id"
        metadata.etag = "test-etag"
        metadata.name = "test.app"
        metadata.fileName = "test.key"
        metadata.fileNameView = "test.key"
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.urlBase = Self.account.serverUrl
        metadata.userId = Self.account.username
        metadata.user = Self.account.username
        metadata.date = .init()
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = UTType.bundle.identifier

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: MockRemoteInterface(account: Self.account)
        )
        XCTAssertEqual(item.contentType, UTType.bundle)
    }

    func testMetadataUnixFolderContentType() {
        let metadata = ItemMetadata()
        metadata.ocId = "test-id"
        metadata.etag = "test-etag"
        metadata.name = "test"
        metadata.fileName = "test"
        metadata.fileNameView = "test"
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.urlBase = Self.account.serverUrl
        metadata.userId = Self.account.username
        metadata.user = Self.account.username
        metadata.date = .init()
        metadata.size = 12
        metadata.directory = true
        metadata.contentType = "httpd/unix-directory"

        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: MockRemoteInterface(account: Self.account)
        )
        XCTAssertEqual(item.contentType, UTType.folder)
    }
}
