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
        /// The reads that write what the server reports.
        enum Read: CaseIterable {
            case depthOne
            case paged
        }

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
                creationDate: Date(timeIntervalSince1970: 1_700_000_000),
                modificationDate: Date(timeIntervalSince1970: 1_700_000_000),
                account: account.ncKitAccount,
                username: account.username,
                userId: account.id,
                serverUrl: account.serverUrl
            )
        }

        private func metadata(_ identifier: String, _ name: String, at serverUrl: String, directory: Bool = false) -> SendableItemMetadata {
            var metadata = remoteItem(identifier, name, at: serverUrl, directory: directory).toItemMetadata(account: account)
            metadata.uploaded = true
            return metadata
        }

        private func store(_ rows: SendableItemMetadata...) {
            for row in rows {
                manager.addItemMetadata(row)
            }
        }

        /// Write `listing`, a folder followed by its children, through the given read.
        @discardableResult
        private func write(_ listing: [SendableItemMetadata], through read: Read) -> ChangeSet? {
            switch read {
                case .depthOne:
                    return manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: listing[0].serverUrl + "/" + listing[0].fileName, updatedMetadatas: listing, keepExistingDownloadState: true)
                case .paged:
                    let files = listing.map { metadata in
                        let item = remoteItem(metadata.ocId, metadata.fileName, at: metadata.serverUrl, directory: metadata.directory)
                        // `toNKFile` takes the parent path from the parent item.
                        let slash = metadata.serverUrl.lastIndex(of: "/")!
                        item.parent = remoteItem("parent", String(metadata.serverUrl[metadata.serverUrl.index(after: slash)...]), at: String(metadata.serverUrl[..<slash]), directory: true)
                        return item.toNKFile()
                    }
                    let (_, error) = Enumerator.handlePagedReadResults(files: files, pageIndex: 0, dbManager: manager, log: log)
                    #expect(error == nil)
                    return nil
            }
        }

        private func liveRows(at serverUrl: String, named fileName: String) -> [SendableItemMetadata] {
            manager.itemMetadatas(account: account.ncKitAccount, underServerUrl: serverUrl)
                .filter { $0.serverUrl == serverUrl && $0.fileName == fileName && !$0.deleted }
        }

        /// `Ontwerp` with a downloaded file and a subfolder holding another file, stored below `src`, plus an empty `dst`.
        private func seedFolderBelowSource() {
            var folder = metadata("folder", "Ontwerp", at: filesUrl + "/src", directory: true)
            folder.visitedDirectory = true
            folder.keepDownloaded = true
            var file = metadata("file", "a.indd", at: filesUrl + "/src/Ontwerp")
            file.downloaded = true

            store(
                metadata("src", "src", at: filesUrl, directory: true),
                metadata("dst", "dst", at: filesUrl, directory: true),
                folder,
                file,
                metadata("subfolder", "sub", at: filesUrl + "/src/Ontwerp", directory: true),
                metadata("nested", "b.indd", at: filesUrl + "/src/Ontwerp/sub")
            )
        }

        private func expectDescendantsMoved(to folderPath: String, sourceLocation: SourceLocation = #_sourceLocation) {
            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == folderPath, sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "subfolder")?.serverUrl == folderPath, sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "nested")?.serverUrl == folderPath + "/sub", sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "file")?.downloaded == true, "A moved file must stay downloaded.", sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "folder")?.visitedDirectory == true, "A moved folder must stay visited.", sourceLocation: sourceLocation)
            #expect(manager.itemMetadata(ocId: "folder")?.keepDownloaded == true, "A moved folder must stay pinned.", sourceLocation: sourceLocation)
            #expect(["folder", "file", "subfolder", "nested"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false }, sourceLocation: sourceLocation)
        }

        // MARK: - Writers

        @Test func aDepthOneListingOfTheNewParentMovesTheFolderWithItsDescendants() {
            seedFolderBelowSource()
            let destination = metadata("dst", "dst", at: filesUrl, directory: true)
            let moved = metadata("folder", "oud", at: filesUrl + "/dst", directory: true)

            let changes = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [destination, moved], keepExistingDownloadState: true)

            #expect(manager.itemMetadata(ocId: "folder")?.serverUrl == filesUrl + "/dst")
            #expect(manager.itemMetadata(ocId: "folder")?.fileName == "oud")
            expectDescendantsMoved(to: filesUrl + "/dst/oud")
            #expect(changes?.updated.contains { $0.ocId == "folder" } == true, "The moved folder must be reported as updated.")
            #expect(changes?.created.contains { $0.ocId == "folder" } == false)
            let updatedIds = Set(changes?.updated.map(\.ocId) ?? [])
            #expect(updatedIds.isSuperset(of: ["file", "subfolder", "nested"]), "The working set scan relies on the moved descendants being reported.")
        }

        /// A folder moved under its own name keeps its etag and dates, so it differs from its stored row only in where it is.
        @Test func aDepthOneListingMovesAFolderThatKeptItsName() {
            seedFolderBelowSource()
            let destination = metadata("dst", "dst", at: filesUrl, directory: true)
            let moved = metadata("folder", "Ontwerp", at: filesUrl + "/dst", directory: true)

            _ = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [destination, moved], keepExistingDownloadState: true)

            expectDescendantsMoved(to: filesUrl + "/dst/Ontwerp")
        }

        @Test func aPagedListingOfTheNewParentMovesTheFolderWithItsDescendants() {
            seedFolderBelowSource()

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("folder", "oud", at: filesUrl + "/dst", directory: true)], through: .paged)

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

        /// A user hit by the original bug has the folder row at its new path and the descendants still at the old one.
        @Test(arguments: Read.allCases)
        func aListingOfTheMovedFolderHealsDescendantsLeftAtTheOldPath(read: Read) {
            seedFolderBelowSource()
            manager.renameItemMetadata(ocId: "folder", newServerUrl: filesUrl + "/dst", newFileName: "oud")

            let changes = write([
                metadata("folder", "oud", at: filesUrl + "/dst", directory: true),
                metadata("file", "a.indd", at: filesUrl + "/dst/oud"),
                metadata("subfolder", "sub", at: filesUrl + "/dst/oud", directory: true)
            ], through: read)

            expectDescendantsMoved(to: filesUrl + "/dst/oud")
            if read == .depthOne {
                #expect(changes?.created.isEmpty == true, "Items moved in are not new.")
            }
        }

        // MARK: - Order and collisions

        /// A subfolder moved out of a folder that the same listing reports as renamed must stay where the listing puts it, whichever entry comes first.
        @Test(arguments: [false, true])
        func aFolderMovedOutOfARenamedFolderStaysWhereItIsListed(renamedFolderListedFirst: Bool) {
            seedFolderBelowSource()
            let source = metadata("src", "src", at: filesUrl, directory: true)
            let renamed = metadata("folder", "Ontwerp2", at: filesUrl + "/src", directory: true)
            let movedOut = metadata("subfolder", "sub", at: filesUrl + "/src", directory: true)
            let children = renamedFolderListedFirst ? [renamed, movedOut] : [movedOut, renamed]

            let changes = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/src", updatedMetadatas: [source] + children, keepExistingDownloadState: true)

            #expect(manager.itemMetadata(ocId: "subfolder")?.serverUrl == filesUrl + "/src")
            #expect(manager.itemMetadata(ocId: "nested")?.serverUrl == filesUrl + "/src/sub")
            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == filesUrl + "/src/Ontwerp2")
            #expect(changes?.updated.filter { $0.ocId == "subfolder" }.map(\.serverUrl) == [filesUrl + "/src"], "The moved subfolder must be reported once, where it is listed.")
        }

        /// On the server `/B/Docs` became `/B/Docs-old` and `/A/Docs` moved to `/B/Docs`, so the contents of the two folders must not mix.
        @Test(arguments: Read.allCases, [false, true])
        func aFolderMovedOntoANameAnotherFolderJustLeftKeepsItsOwnChildren(read: Read, movedFolderListedFirst: Bool) {
            store(
                metadata("A", "A", at: filesUrl, directory: true),
                metadata("B", "B", at: filesUrl, directory: true),
                metadata("docs-b", "Docs", at: filesUrl + "/B", directory: true),
                metadata("b1", "b1.txt", at: filesUrl + "/B/Docs"),
                metadata("docs-a", "Docs", at: filesUrl + "/A", directory: true),
                metadata("a1", "a1.txt", at: filesUrl + "/A/Docs"),
                metadata("a-sub", "sub", at: filesUrl + "/A/Docs", directory: true),
                metadata("a2", "a2.txt", at: filesUrl + "/A/Docs/sub")
            )
            let movedIn = metadata("docs-a", "Docs", at: filesUrl + "/B", directory: true)
            let renamed = metadata("docs-b", "Docs-old", at: filesUrl + "/B", directory: true)
            let children = movedFolderListedFirst ? [movedIn, renamed] : [renamed, movedIn]

            write([metadata("B", "B", at: filesUrl, directory: true)] + children, through: read)

            #expect(manager.itemMetadata(ocId: "b1")?.serverUrl == filesUrl + "/B/Docs-old")
            #expect(manager.itemMetadata(ocId: "a1")?.serverUrl != filesUrl + "/B/Docs-old", "A child of the moved folder must not follow the renamed one.")
            #expect(manager.itemMetadata(ocId: "a2")?.serverUrl.hasPrefix(filesUrl + "/B/Docs-old") == false)
            #expect(["b1", "a1", "a-sub", "a2"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false })

            if read == .depthOne {
                #expect(manager.itemMetadata(ocId: "a1")?.serverUrl == filesUrl + "/B/Docs", "One listing moves both folders, whichever comes first.")
                #expect(manager.itemMetadata(ocId: "a2")?.serverUrl == filesUrl + "/B/Docs/sub")
            }
        }

        /// On the server `/dst/Project` was deleted and `/src/Project` moved to `/dst/Project`; both held a `Report.docx`.
        @Test func aFolderMovedOntoADeletedFolderReplacesItsChildren() {
            store(
                metadata("src", "src", at: filesUrl, directory: true),
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-project", "Project", at: filesUrl + "/dst", directory: true),
                metadata("old-report", "Report.docx", at: filesUrl + "/dst/Project"),
                metadata("new-project", "Project", at: filesUrl + "/src", directory: true),
                metadata("new-report", "Report.docx", at: filesUrl + "/src/Project")
            )

            let changes = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("new-project", "Project", at: filesUrl + "/dst", directory: true)
            ], keepExistingDownloadState: true)

            #expect(manager.itemMetadata(ocId: "old-project")?.deleted == true)
            #expect(liveRows(at: filesUrl + "/dst/Project", named: "Report.docx").map(\.ocId) == ["new-report"])
            #expect(changes?.updated.contains { $0.ocId == "old-report" } == false, "A child of the deleted folder must not be reported as moved.")
        }

        /// A folder whose read returned 404 is tombstoned with its children, and the listing of its new path must bring the children back.
        @Test func aListingRevivesTheChildrenOfAFolderTombstonedBeforeTheMove() {
            seedFolderBelowSource()
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "folder")

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("folder", "oud", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            write([
                metadata("folder", "oud", at: filesUrl + "/dst", directory: true),
                metadata("file", "a.indd", at: filesUrl + "/dst/oud"),
                metadata("subfolder", "sub", at: filesUrl + "/dst/oud", directory: true)
            ], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "folder")?.deleted == false)
            #expect(manager.itemMetadata(ocId: "file")?.deleted == false)
            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == filesUrl + "/dst/oud")
            #expect(manager.itemMetadata(ocId: "subfolder")?.deleted == false)
        }

        /// Pins the behaviour for a folder shared twice (#10106): each listing moves the one row, and its descendants follow the last listing.
        @Test func aFolderListedUnderTwoParentsFollowsTheLastListing() {
            seedFolderBelowSource()
            let source = metadata("src", "src", at: filesUrl, directory: true)
            let atSource = metadata("folder", "Ontwerp", at: filesUrl + "/src", directory: true)
            let destination = metadata("dst", "dst", at: filesUrl, directory: true)
            let atDestination = metadata("folder", "Ontwerp", at: filesUrl + "/dst", directory: true)

            _ = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/dst", updatedMetadatas: [destination, atDestination], keepExistingDownloadState: true)
            expectDescendantsMoved(to: filesUrl + "/dst/Ontwerp")

            _ = manager.depth1ReadUpdateItemMetadatas(account: account.ncKitAccount, serverUrl: filesUrl + "/src", updatedMetadatas: [source, atSource], keepExistingDownloadState: true)
            expectDescendantsMoved(to: filesUrl + "/src/Ontwerp")
        }
    }
}
