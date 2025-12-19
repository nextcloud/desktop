//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudKit
import XCTest

final class NKFileExtensionsTests: NextcloudFileProviderKitTestCase {
    static let account = Account(user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd")

    ///
    /// Mock an `NKFile` for the tests in here.
    ///
    private func createNKFile(ocId: String = "id1", serverUrl: String? = nil, fileName: String = "file.txt", directory: Bool = false, userId: String? = nil, urlBase: String? = nil) -> NKFile {
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
            serverUrl: "https://mock.nc.com/remote.php/dav/files/testUserId", // Special root value
            fileName: NextcloudKit.shared.nkCommonInstance.rootFileName, // Special root value
            directory: false // NextcloudKit sometimes marks the root as not a directory
        )

        // 2. Act
        let metadata = rootNKFile.toItemMetadata()

        // 3. Assert
        // The `rootRequiresFixup` logic should trigger.
        XCTAssertEqual(metadata.serverUrl, Self.account.davFilesUrl, "The serverUrl for the root should be corrected to the full WebDAV path.")
        XCTAssertEqual(metadata.ocId, NSFileProviderItemIdentifier.rootContainer.rawValue, "The ocId for the root should be mapped to a file provider root container.")
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

    // MARK: - isDirectoryToRead(_:directoryToRead:)

    func testIsDirectoryToReadForRoot() {
        let item = createNKFile(ocId: NextcloudKit.shared.nkCommonInstance.rootFileName, serverUrl: Self.account.davFilesUrl, fileName: NextcloudKit.shared.nkCommonInstance.rootFileName)
        let items = [NKFile]()
        let result = items.isDirectoryToRead(item, directoryToRead: Self.account.davFilesUrl)
        XCTAssertTrue(result)
    }

    func testIsDirectoryToReadForDirectoryInRoot() {
        let item = createNKFile(ocId: "some", serverUrl: Self.account.davFilesUrl, fileName: "Subdirectory", directory: true)
        let items = [NKFile]()
        let result = items.isDirectoryToRead(item, directoryToRead: "\(Self.account.davFilesUrl)/Subdirectory")
        XCTAssertTrue(result)
    }

    func testIsDirectoryToReadForFileInRoot() {
        let item = createNKFile(ocId: "some", serverUrl: Self.account.davFilesUrl, fileName: "File", directory: false)
        let items = [NKFile]()
        let result = items.isDirectoryToRead(item, directoryToRead: Self.account.davFilesUrl)
        XCTAssertFalse(result)
    }

    func testIsDirectoryToReadForFileInSubdirectory() {
        let subdirectoryURL = "\(Self.account.davFilesUrl)/Subdirectory"
        let item = createNKFile(ocId: "some", serverUrl: "\(subdirectoryURL)/File", fileName: "File", directory: false)
        let items = [NKFile]()
        let result = items.isDirectoryToRead(item, directoryToRead: subdirectoryURL)
        XCTAssertFalse(result)
    }

    // MARK: - toSendableDirectoryMetadata(account:directoryToRead:) Tests

    func testToSendableDirectoryMetadataHandlesRootAsTargetCorrectly() async throws {
        // 1. Arrange: Create an array of NKFiles where the first item is the special root.
        let rootNKFile = createNKFile(
            ocId: "rootId", // This will be overridden by the logic
            serverUrl: Self.account.davFilesUrl,
            fileName: NextcloudKit.shared.nkCommonInstance.rootFileName,
            directory: false // Mimic NextcloudKit behavior
        )

        let childNKFile = createNKFile(
            ocId: "child1",
            serverUrl: "\(Self.account.davFilesUrl)/document.txt",
            fileName: "document.txt"
        )

        let files = [rootNKFile, childNKFile]

        // 2. Act
        let result = await files.toSendableDirectoryMetadata(account: Self.account, directoryToRead: Self.account.davFilesUrl)

        // 3. Assert
        let unwrappedResult = try XCTUnwrap(result)

        // The logic should identify the first item as the root and fix its properties.
        XCTAssertEqual(unwrappedResult.root.ocId, NSFileProviderItemIdentifier.rootContainer.rawValue, "The target directory's ocId should be corrected to the root container identifier.")
        XCTAssertTrue(unwrappedResult.root.directory, "The target directory should be correctly marked as a directory, even if the NKFile was not.")

        // Ensure the child item is processed correctly.
        XCTAssertEqual(unwrappedResult.files.count, 1, "There should be one file metadata object.")
        XCTAssertEqual(unwrappedResult.files.first?.ocId, "child1")
        XCTAssertTrue(unwrappedResult.directories.isEmpty, "The child is a file, so child directories should be empty.")
    }

    func testToSendableDirectoryMetadataHandlesNormalFolderAsTarget() async throws {
        // 1. Arrange: Create an array for a normal folder and its children.
        let parentFolderNKFile = createNKFile(
            ocId: "folder1",
            serverUrl: Self.account.davFilesUrl,
            fileName: "MyFolder",
            directory: true
        )

        let childFileNKFile = createNKFile(
            ocId: "file1",
            serverUrl: "\(Self.account.davFilesUrl)/MyFolder",
            fileName: "report.docx"
        )

        let childDirNKFile = createNKFile(
            ocId: "dir2",
            serverUrl: "\(Self.account.davFilesUrl)/MyFolder",
            fileName: "Subfolder",
            directory: true
        )

        let files = [parentFolderNKFile, childFileNKFile, childDirNKFile]

        // 2. Act
        let result = await files.toSendableDirectoryMetadata(account: Self.account, directoryToRead: "\(Self.account.davFilesUrl)/MyFolder")

        // 3. Assert
        let unwrappedResult = try XCTUnwrap(result)

        // The root fixup logic should NOT trigger for a normal folder.
        XCTAssertEqual(unwrappedResult.root.ocId, "folder1")
        XCTAssertEqual(unwrappedResult.root.fileName, "MyFolder")
        XCTAssertTrue(unwrappedResult.root.directory)

        // Check children processing
        XCTAssertEqual(unwrappedResult.files.count, 2, "Should have two child metadata objects.")
        XCTAssertEqual(unwrappedResult.directories.count, 1, "Should identify one child directory.")
        XCTAssertEqual(unwrappedResult.directories.first?.ocId, "dir2")
    }
}
