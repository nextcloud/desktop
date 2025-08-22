//
//  NKFileExtensionTests.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 16/6/25.
//

import XCTest
import FileProvider
import NextcloudKit
@testable import NextcloudFileProviderKit

final class NKFileExtensionsTests: NextcloudFileProviderKitTestCase {

    static let account = Account(
        user: "testUser",
        id: "testUserId",
        serverUrl: "https://mock.nc.com",
        password: "abcd"
    )

    /// Creates a mock NKFile.
    private func createNKFile(
        ocId: String = "id1",
        serverUrl: String? = nil,
        fileName: String = "file.txt",
        directory: Bool = false,
        userId: String? = nil,
        urlBase: String? = nil
    ) -> NKFile {
        var file = NKFile()
        file.ocId = ocId
        file.serverUrl = serverUrl ?? Self.account.davFilesUrl
        file.fileName = fileName
        file.directory = directory
        file.userId = userId ?? Self.account.id
        file.urlBase = urlBase ?? Self.account.serverUrl
        // Add other necessary properties with default values
        file.account = Self.account.ncKitAccount
        file.date = Date()
        file.creationDate = Date()
        file.etag = "etag"
        return file
    }

    // MARK: - toItemMetadata() Tests

    func testToItemMetadataHandlesRootFixupCorrectly() {
        // 1. Arrange: Create an NKFile representing the root container,
        // which has a special serverUrl and fileName.
        let rootNKFile = createNKFile(
            ocId: "rootId",
            serverUrl: "..", // Special root value
            fileName: ".",   // Special root value
            directory: false // NextcloudKit sometimes marks the root as not a directory
        )

        // 2. Act
        let metadata = rootNKFile.toItemMetadata()

        // 3. Assert
        // The `rootRequiresFixup` logic should trigger.
        XCTAssertEqual(
            metadata.serverUrl,
            Self.account.davFilesUrl,
            "The serverUrl for the root should be corrected to the full WebDAV path."
        )
        XCTAssertEqual(
            metadata.fileName,
            "",
            "The fileName for the root should be corrected to an empty string."
        )
    }

    func testToItemMetadataHandlesStandardFileCorrectly() {
        // 1. Arrange: Create a standard NKFile for a regular file.
        let standardNKFile = createNKFile(
            ocId: "file123",
            serverUrl: Self.account.davFilesUrl + "/photos",
            fileName: "image.jpg"
        )

        // 2. Act
        let metadata = standardNKFile.toItemMetadata()

        // 3. Assert
        // The `rootRequiresFixup` logic should NOT trigger.
        XCTAssertEqual(
            metadata.serverUrl,
            Self.account.davFilesUrl + "/photos",
            "The serverUrl for a standard file should remain unchanged."
        )
        XCTAssertEqual(
            metadata.fileName,
            "image.jpg",
            "The fileName for a standard file should remain unchanged."
        )
    }

    // MARK: - toDirectoryReadMetadatas(account:) Tests

    func testToDirectoryReadMetadatasHandlesRootAsTargetCorrectly() async {
        // 1. Arrange: Create an array of NKFiles where the first item is the special root.
        let rootNKFile = createNKFile(
            ocId: "rootId", // This will be overridden by the logic
            serverUrl: "..",
            fileName: ".",
            directory: false // Mimic NextcloudKit behavior
        )
        let childNKFile = createNKFile(
            ocId: "child1",
            serverUrl: Self.account.davFilesUrl,
            fileName: "document.txt"
        )

        let files = [rootNKFile, childNKFile]

        // 2. Act
        let result = await files.toDirectoryReadMetadatas(account: Self.account)

        // 3. Assert
        XCTAssertNotNil(result, "The conversion should succeed.")
        guard let result else { return }

        // The logic should identify the first item as the root and fix its properties.
        XCTAssertEqual(
            result.directoryMetadata.ocId,
            NSFileProviderItemIdentifier.rootContainer.rawValue,
            "The target directory's ocId should be corrected to the root container identifier."
        )
        XCTAssertTrue(
            result.directoryMetadata.directory,
            "The target directory should be correctly marked as a directory, even if the NKFile was not."
        )

        // Ensure the child item is processed correctly.
        XCTAssertEqual(result.metadatas.count, 1, "There should be one child metadata object.")
        XCTAssertEqual(result.metadatas.first?.ocId, "child1")
        XCTAssertTrue(result.childDirectoriesMetadatas.isEmpty, "The child is a file, so child directories should be empty.")
    }

    func testToDirectoryReadMetadatasHandlesNormalFolderAsTarget() async {
        // 1. Arrange: Create an array for a normal folder and its children.
        let parentFolderNKFile = createNKFile(
            ocId: "folder1",
            serverUrl: Self.account.davFilesUrl,
            fileName: "MyFolder",
            directory: true
        )
        let childFileNKFile = createNKFile(
            ocId: "file1",
            serverUrl: Self.account.davFilesUrl + "/MyFolder",
            fileName: "report.docx"
        )
        let childDirNKFile = createNKFile(
            ocId: "dir2",
            serverUrl: Self.account.davFilesUrl + "/MyFolder",
            fileName: "Subfolder",
            directory: true
        )

        let files = [parentFolderNKFile, childFileNKFile, childDirNKFile]

        // 2. Act
        let result = await files.toDirectoryReadMetadatas(account: Self.account)

        // 3. Assert
        XCTAssertNotNil(result)
        guard let result else { return }

        // The root fixup logic should NOT trigger for a normal folder.
        XCTAssertEqual(result.directoryMetadata.ocId, "folder1")
        XCTAssertEqual(result.directoryMetadata.fileName, "MyFolder")
        XCTAssertTrue(result.directoryMetadata.directory)

        // Check children processing
        XCTAssertEqual(result.metadatas.count, 2, "Should have two child metadata objects.")
        XCTAssertEqual(result.childDirectoriesMetadatas.count, 1, "Should identify one child directory.")
        XCTAssertEqual(result.childDirectoriesMetadatas.first?.ocId, "dir2")
    }
}

