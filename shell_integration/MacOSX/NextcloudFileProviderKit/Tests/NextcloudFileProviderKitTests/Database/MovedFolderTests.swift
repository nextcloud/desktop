//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
@testable import TestInterface

extension DatabaseTestSuites {
    /// A folder the server moved must take its stored descendants along, whichever read reports the move.
    @Suite("Moved folder")
    struct MovedFolderTests {
        let manager = DatabaseTestSuites.makeManager()
        let account = DatabaseTestSuites.account
        let log = FileProviderLogMock()

        var filesUrl: String {
            account.davFilesUrl
        }

        private func remoteItem(_ identifier: String, _ name: String, at serverUrl: String, directory: Bool = false) -> MockRemoteItem {
            MockRemoteItem(
                identifier: identifier,
                name: name,
                remotePath: serverUrl + "/" + name,
                directory: directory,
                account: account.ncKitAccount,
                username: account.username,
                userId: account.id,
                serverUrl: account.serverUrl
            )
        }

        /// `Ontwerp` with a downloaded file and a subfolder holding another file, stored below `src`, plus an empty `dst`.
        private func seedFolderBelowSource() {
            var source = remoteItem("src", "src", at: filesUrl, directory: true).toItemMetadata(account: account)
            source.uploaded = true
            var destination = remoteItem("dst", "dst", at: filesUrl, directory: true).toItemMetadata(account: account)
            destination.uploaded = true
            var folder = remoteItem("folder", "Ontwerp", at: filesUrl + "/src", directory: true).toItemMetadata(account: account)
            folder.uploaded = true
            folder.visitedDirectory = true
            var file = remoteItem("file", "a.indd", at: filesUrl + "/src/Ontwerp").toItemMetadata(account: account)
            file.uploaded = true
            file.downloaded = true
            var subfolder = remoteItem("subfolder", "sub", at: filesUrl + "/src/Ontwerp", directory: true).toItemMetadata(account: account)
            subfolder.uploaded = true
            var nestedFile = remoteItem("nested", "b.indd", at: filesUrl + "/src/Ontwerp/sub").toItemMetadata(account: account)
            nestedFile.uploaded = true

            for row in [source, destination, folder, file, subfolder, nestedFile] {
                manager.addItemMetadata(row)
            }
        }

        private func expectDescendantsMoved(to folderPath: String, sourceLocation: SourceLocation = #_sourceLocation) {
            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == folderPath, sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "subfolder")?.serverUrl == folderPath, sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "nested")?.serverUrl == folderPath + "/sub", sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "file")?.downloaded == true, "A moved file must stay downloaded.", sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "folder")?.visitedDirectory == true, "A moved folder must stay visited.", sourceLocation: sourceLocation)
            #expect(["folder", "file", "subfolder", "nested"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false }, sourceLocation: sourceLocation)
        }

        @Test func aDepthOneListingOfTheNewParentMovesTheFolderWithItsDescendants() {
            seedFolderBelowSource()
            let destination = remoteItem("dst", "dst", at: filesUrl, directory: true).toItemMetadata(account: account)
            let moved = remoteItem("folder", "oud", at: filesUrl + "/dst", directory: true).toItemMetadata(account: account)

            let changes = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [destination, moved], keepExistingDownloadState: true)

            #expect(manager.itemMetadata(ocId: "folder")?.serverUrl == filesUrl + "/dst")
            #expect(manager.itemMetadata(ocId: "folder")?.fileName == "oud")
            expectDescendantsMoved(to: filesUrl + "/dst/oud")
            #expect(changes?.updated.contains { $0.ocId == "folder" } == true, "The moved folder must be reported as updated.")
            #expect(changes?.created.contains { $0.ocId == "folder" } == false)
        }

        /// A folder moved under its own name keeps its etag, so it differs from its stored row only in where it is.
        @Test func aDepthOneListingMovesAFolderThatKeptItsName() {
            seedFolderBelowSource()
            let destination = remoteItem("dst", "dst", at: filesUrl, directory: true).toItemMetadata(account: account)
            let moved = remoteItem("folder", "Ontwerp", at: filesUrl + "/dst", directory: true).toItemMetadata(account: account)

            _ = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [destination, moved], keepExistingDownloadState: true)

            expectDescendantsMoved(to: filesUrl + "/dst/Ontwerp")
        }

        @Test func aPagedListingOfTheNewParentMovesTheFolderWithItsDescendants() {
            seedFolderBelowSource()
            let root = MockRemoteItem.rootItem(account: account)
            let destination = remoteItem("dst", "dst", at: filesUrl, directory: true)
            let moved = remoteItem("folder", "oud", at: filesUrl + "/dst", directory: true)
            destination.parent = root
            moved.parent = destination

            let (_, error) = Enumerator.handlePagedReadResults(files: [destination.toNKFile(), moved.toNKFile()], pageIndex: 0, dbManager: manager, log: log)

            #expect(error == nil)
            #expect(manager.itemMetadata(ocId: "folder")?.fileName == "oud")
            expectDescendantsMoved(to: filesUrl + "/dst/oud")
        }

        @Test func aSingleItemReadOfTheMovedFolderMovesItsDescendants() async {
            seedFolderBelowSource()
            let root = MockRemoteItem.rootItem(account: account)
            let destination = remoteItem("dst", "dst", at: filesUrl, directory: true)
            let moved = remoteItem("folder", "oud", at: filesUrl + "/dst", directory: true)
            root.children = [destination]
            destination.parent = root
            destination.children = [moved]
            moved.parent = destination
            let remoteInterface = MockRemoteInterface(account: account, rootItem: root)

            let result = await Enumerator.readServerUrl(filesUrl + "/dst/oud", account: account, remoteInterface: remoteInterface, dbManager: manager, depth: .target, log: log)

            #expect(result.error == nil)
            #expect(manager.itemMetadata(ocId: "folder")?.fileName == "oud")
            expectDescendantsMoved(to: filesUrl + "/dst/oud")
        }

        /// A subfolder moved out of a folder that the same listing reports as renamed must stay where the listing puts it, whichever entry comes first.
        @Test(arguments: [false, true])
        func aFolderMovedOutOfARenamedFolderStaysWhereItIsListed(renamedFolderListedFirst: Bool) {
            seedFolderBelowSource()
            let source = remoteItem("src", "src", at: filesUrl, directory: true).toItemMetadata(account: account)
            let renamed = remoteItem("folder", "Ontwerp2", at: filesUrl + "/src", directory: true).toItemMetadata(account: account)
            let movedOut = remoteItem("subfolder", "sub", at: filesUrl + "/src", directory: true).toItemMetadata(account: account)
            let children = renamedFolderListedFirst ? [renamed, movedOut] : [movedOut, renamed]

            let changes = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/src", updatedMetadatas: [source] + children, keepExistingDownloadState: true)

            #expect(manager.itemMetadata(ocId: "subfolder")?.serverUrl == filesUrl + "/src")
            #expect(manager.itemMetadata(ocId: "nested")?.serverUrl == filesUrl + "/src/sub")
            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == filesUrl + "/src/Ontwerp2")
            #expect(changes?.updated.filter { $0.ocId == "subfolder" }.map(\.serverUrl) == [filesUrl + "/src"], "The moved subfolder must be reported once, where it is listed.")
        }

        /// Pins the behaviour for a folder shared twice (#10106): each listing moves the one row, and its descendants follow the last listing.
        @Test func aFolderListedUnderTwoParentsFollowsTheLastListing() {
            seedFolderBelowSource()
            let source = remoteItem("src", "src", at: filesUrl, directory: true).toItemMetadata(account: account)
            let atSource = remoteItem("folder", "Ontwerp", at: filesUrl + "/src", directory: true).toItemMetadata(account: account)
            let destination = remoteItem("dst", "dst", at: filesUrl, directory: true).toItemMetadata(account: account)
            let atDestination = remoteItem("folder", "Ontwerp", at: filesUrl + "/dst", directory: true).toItemMetadata(account: account)

            _ = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [destination, atDestination], keepExistingDownloadState: true)
            expectDescendantsMoved(to: filesUrl + "/dst/Ontwerp")

            _ = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/src", updatedMetadatas: [source, atSource], keepExistingDownloadState: true)
            expectDescendantsMoved(to: filesUrl + "/src/Ontwerp")
        }
    }
}
