//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
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
            #expect(manager.itemMetadata(ocId: "a1")?.serverUrl == filesUrl + "/B/Docs", "One listing moves both folders, whichever comes first.")
            #expect(manager.itemMetadata(ocId: "a2")?.serverUrl == filesUrl + "/B/Docs/sub")
        }

        /// On the server the folders `/B/One` and `/B/Two` swapped names, so each must keep its own child.
        @Test(arguments: Read.allCases, [false, true])
        func foldersThatSwappedNamesKeepTheirOwnChildren(read: Read, oneListedFirst: Bool) {
            store(
                metadata("B", "B", at: filesUrl, directory: true),
                metadata("one", "One", at: filesUrl + "/B", directory: true),
                metadata("one-child", "one.txt", at: filesUrl + "/B/One"),
                metadata("two", "Two", at: filesUrl + "/B", directory: true),
                metadata("two-child", "two.txt", at: filesUrl + "/B/Two")
            )
            let one = metadata("one", "Two", at: filesUrl + "/B", directory: true)
            let two = metadata("two", "One", at: filesUrl + "/B", directory: true)

            write([metadata("B", "B", at: filesUrl, directory: true)] + (oneListedFirst ? [one, two] : [two, one]), through: read)

            #expect(manager.itemMetadata(ocId: "one-child")?.serverUrl == filesUrl + "/B/Two")
            #expect(manager.itemMetadata(ocId: "two-child")?.serverUrl == filesUrl + "/B/One")
            #expect(["one", "one-child", "two", "two-child"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false })
        }

        /// On the server `/dst/Docs` was replaced by a new folder of the same name, so its children must not appear in the new one.
        @Test(arguments: Read.allCases)
        func aNewFolderAtTheLocationOfAnotherDoesNotTakeItsChildren(read: Read) {
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("old-file", "old.txt", at: filesUrl + "/dst/Docs")
            )

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: read)

            #expect(liveRows(at: filesUrl + "/dst/Docs", named: "old.txt").isEmpty)
            #expect(liveRows(at: filesUrl + "/dst", named: "Docs").map(\.ocId) == ["new-docs"])
            let parkedAt = manager.itemMetadata(ocId: "old-docs")?.syncTime ?? .distantPast
            let parkTimeTolerance: TimeInterval = 60
            #expect(abs(parkedAt.timeIntervalSinceNow) < parkTimeTolerance, "Parking stamps the row with the time it was parked.")
        }

        /// On the server `/dst/Docs` moved to `/dst/Archive/Docs-2023` and `/dst/Drafts/Docs` moved to `/dst/Docs`, and `/dst` is listed before `/dst/Archive`.
        /// The old folder may still be marked as downloading, because a folder fetch keeps that status for its whole run.
        @Test(arguments: [false, true], [false, true])
        func aFolderFoundLaterInAnotherParentTakesOnlyItsOwnChildren(oldFolderDownloading: Bool, deletionsWrittenBack: Bool) {
            var oldDocs = metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true)
            if oldFolderDownloading {
                oldDocs.status = Status.downloading.rawValue
            }
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("archive", "Archive", at: filesUrl + "/dst", directory: true),
                metadata("drafts", "Drafts", at: filesUrl + "/dst", directory: true),
                oldDocs,
                metadata("old-file", "old.txt", at: filesUrl + "/dst/Docs"),
                metadata("new-docs", "Docs", at: filesUrl + "/dst/Drafts", directory: true),
                metadata("new-file", "new.txt", at: filesUrl + "/dst/Drafts/Docs")
            )

            let changes = write([
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("archive", "Archive", at: filesUrl + "/dst", directory: true),
                metadata("drafts", "Drafts", at: filesUrl + "/dst", directory: true),
                metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)
            ], through: .depthOne)
            if deletionsWrittenBack {
                // The working set scan writes every reported deletion back like this.
                changes?.deleted.forEach { manager.addItemMetadata($0) }
            }
            write([
                metadata("archive", "Archive", at: filesUrl + "/dst", directory: true),
                metadata("old-docs", "Docs-2023", at: filesUrl + "/dst/Archive", directory: true)
            ], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "new-file")?.serverUrl == filesUrl + "/dst/Docs")
            #expect(manager.itemMetadata(ocId: "old-file")?.serverUrl == filesUrl + "/dst/Archive/Docs-2023")
            #expect(["old-docs", "old-file", "new-docs", "new-file"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false })
        }

        /// An enumerator of the old `/dst/Docs` reads that path after the server put another folder there, so the two folders must stay apart when the old one turns up in `/dst/Archive`.
        @Test(arguments: [false, true])
        func aReadOfAPathAnotherFolderNowHoldsKeepsTheTwoFoldersApart(newFolderStoredBefore: Bool) {
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("archive", "Archive", at: filesUrl + "/dst", directory: true),
                metadata("drafts", "Drafts", at: filesUrl + "/dst", directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("old-file", "old.txt", at: filesUrl + "/dst/Docs")
            )
            if newFolderStoredBefore {
                store(metadata("new-docs", "Docs", at: filesUrl + "/dst/Drafts", directory: true))
            }

            let read = write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true), metadata("new-file", "new.txt", at: filesUrl + "/dst/Docs")], through: .depthOne)
            let reported = newFolderStoredBefore ? read?.updated : read?.created
            #expect(reported?.first { $0.ocId == "new-docs" }?.serverUrl == filesUrl + "/dst", "The folder read is reported where it is now.")
            write([
                metadata("archive", "Archive", at: filesUrl + "/dst", directory: true),
                metadata("old-docs", "Docs-2023", at: filesUrl + "/dst/Archive", directory: true)
            ], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "new-file")?.serverUrl == filesUrl + "/dst/Docs")
            #expect(manager.itemMetadata(ocId: "new-file")?.deleted == false)
            #expect(manager.itemMetadata(ocId: "old-file")?.serverUrl == filesUrl + "/dst/Archive/Docs-2023")
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

        /// A listing of `/dst` already marked `/dst/Project` deleted where it is stored, with its file still live below it, before `/src/Project` moved there.
        @Test(arguments: Read.allCases)
        func aFolderMovedOntoAFolderDeletedEarlierDoesNotTakeItsChildren(read: Read) {
            store(
                metadata("src", "src", at: filesUrl, directory: true),
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-project", "Project", at: filesUrl + "/dst", directory: true),
                metadata("old-report", "Report.docx", at: filesUrl + "/dst/Project"),
                metadata("new-project", "Project", at: filesUrl + "/src", directory: true),
                metadata("new-report", "Report.docx", at: filesUrl + "/src/Project")
            )
            write([metadata("dst", "dst", at: filesUrl, directory: true)], through: .depthOne)
            #expect(manager.itemMetadata(ocId: "old-project")?.deleted == true)

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("new-project", "Project", at: filesUrl + "/dst", directory: true)], through: read)

            #expect(liveRows(at: filesUrl + "/dst/Project", named: "Report.docx").map(\.ocId) == ["new-report"])
        }

        /// A file with a pending upload moves along with its parked folder and keeps its status.
        @Test func aFileWithAPendingUploadMovesWithItsParkedFolder() {
            var pending = metadata("old-file", "old.txt", at: filesUrl + "/dst/Docs")
            pending.status = Status.uploading.rawValue
            store(
                metadata("src", "src", at: filesUrl, directory: true),
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                pending,
                metadata("new-docs", "Docs", at: filesUrl + "/src", directory: true)
            )

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "old-docs")?.fileName == "Docs\u{1}old-docs")
            #expect(manager.itemMetadata(ocId: "old-file")?.serverUrl == filesUrl + "/dst/Docs\u{1}old-docs")
            #expect(manager.itemMetadata(ocId: "old-file")?.status == Status.uploading.rawValue)
            #expect(manager.itemMetadata(ocId: "new-docs")?.serverUrl == filesUrl + "/dst")
        }

        /// The user deleted `/P/Final` and renamed `Draft` to `Final` before the deletion was acknowledged, so both folder rows share one path.
        @Test(arguments: Read.allCases)
        func aFolderStoredAtTheLocationOfADeletedFolderKeepsItsChildrenInPlace(read: Read) {
            store(
                metadata("p", "P", at: filesUrl, directory: true),
                metadata("old-final", "Final", at: filesUrl + "/P", directory: true),
                metadata("old-report", "Old.docx", at: filesUrl + "/P/Final"),
                metadata("draft", "Draft", at: filesUrl + "/P", directory: true),
                metadata("report", "Report.docx", at: filesUrl + "/P/Draft")
            )
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "old-final")
            _ = manager.renameDirectoryAndPropagateToChildren(ocId: "draft", newServerUrl: filesUrl + "/P", newFileName: "Final")
            var locked = metadata("report", "Report.docx", at: filesUrl + "/P/Final")
            locked.lockToken = "token"
            var lockFile = metadata("lock-file", "~$Report.docx", at: filesUrl + "/P/Final")
            lockFile.isLockFileOfLocalOrigin = true
            lockFile.uploaded = false
            lockFile.classFile = "lock"
            store(locked, lockFile)

            write([metadata("draft", "Final", at: filesUrl + "/P", directory: true), metadata("report", "Report.docx", at: filesUrl + "/P/Final")], through: read)

            #expect(manager.itemMetadata(ocId: "old-final")?.fileName == "Final")
            #expect(manager.itemMetadata(ocId: "report")?.serverUrl == filesUrl + "/P/Final")
            #expect(manager.itemMetadata(ocId: "report")?.lockToken == "token")
            #expect(manager.itemMetadata(ocId: "lock-file")?.serverUrl == filesUrl + "/P/Final")
        }

        /// On the server `/dst/Docs` was renamed to `Docs-2023` and a new `/dst/Docs` took `report.pdf` from it, and the new folder is read before `/dst` is listed.
        @Test func aFileMovedIntoTheFolderNowAtTheReadPathStaysInIt() {
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("report", "report.pdf", at: filesUrl + "/dst/Docs"),
                metadata("other", "other.txt", at: filesUrl + "/dst/Docs")
            )

            let read = write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true), metadata("report", "report.pdf", at: filesUrl + "/dst/Docs")], through: .depthOne)
            #expect(read?.updated.contains { $0.ocId == "report" } == true, "The file now has another parent folder.")
            #expect(read?.deleted.contains { $0.ocId == "other" } == false, "The file stays in the renamed folder.")
            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("old-docs", "Docs-2023", at: filesUrl + "/dst", directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "report")?.serverUrl == filesUrl + "/dst/Docs")
            #expect(manager.itemMetadata(ocId: "other")?.serverUrl == filesUrl + "/dst/Docs-2023")
            #expect(["old-docs", "new-docs", "report", "other"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false })
        }

        /// A delete of a parked folder or of a file below it waits until a listing has placed the folder again, so no request goes to the holding path.
        @Test(arguments: ["other", "old-docs"])
        func aDeleteBelowAParkedFolderWaitsForItsPlacement(identifier: String) async throws {
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("other", "other.txt", at: filesUrl + "/dst/Docs")
            )
            write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))
            remoteInterface.deleteError = NKError(statusCode: 404, fallbackDescription: "Not Found")
            let parked = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier(identifier), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))

            let error = await parked.delete(dbManager: manager)

            #expect((error as? NSFileProviderError)?.code == .cannotSynchronize)
            #expect(remoteInterface.lastDeleteRemotePath == nil)
            #expect(manager.itemMetadata(ocId: "other")?.deleted == false)

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("old-docs", "Docs-2023", at: filesUrl + "/dst", directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let placed = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier(identifier), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))

            #expect(await placed.delete(dbManager: manager) == nil)
            #expect(remoteInterface.lastDeleteRemotePath == (identifier == "other" ? filesUrl + "/dst/Docs-2023/other.txt" : filesUrl + "/dst/Docs-2023"))
        }

        /// A lock file below a parked folder is kept, and no unlock goes to the holding path, until a listing has placed the folder again.
        @Test func aLockFileBelowAParkedFolderIsKeptUntilItsPlacement() async throws {
            var lockFile = metadata("lock-file", "~$Report.docx", at: filesUrl + "/dst/Docs")
            lockFile.isLockFileOfLocalOrigin = true
            lockFile.uploaded = false
            lockFile.classFile = "lock"
            store(metadata("dst", "dst", at: filesUrl, directory: true), metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true), metadata("report", "Report.docx", at: filesUrl + "/dst/Docs"), lockFile)
            write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))
            let item = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier("lock-file"), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))

            let error = await item.delete(dbManager: manager)

            #expect((error as? NSFileProviderError)?.code == .cannotSynchronize)
            #expect(manager.itemMetadata(ocId: "lock-file")?.deleted == false, "The lock file row is removed only before the unlock is sent.")
        }

        /// A folder below a parked folder is not read at the holding path, so its enumerator asks to be retried instead of reading a path the server never had.
        @Test func aFolderBelowAParkedFolderIsNotReadAtTheHoldingPath() async throws {
            var child = metadata("child", "C", at: filesUrl + "/dst/Docs", directory: true)
            child.visitedDirectory = true
            store(metadata("dst", "dst", at: filesUrl, directory: true), metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true), child)
            write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))
            let recorder = EnumeratePathRecorder()
            remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
                recorder.add(remotePath)
            }
            let observer = try MockChangeObserver(enumerator: Enumerator(enumeratedItemIdentifier: NSFileProviderItemIdentifier("child"), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))

            let error = await #expect(throws: NSFileProviderError.self) {
                try await observer.enumerateChanges()
            }

            #expect(error?.code == .cannotSynchronize)
            #expect(recorder.paths.isEmpty)
            #expect(manager.itemMetadata(ocId: "child")?.deleted == false)
        }

        /// The working set scan does not read a downloaded file at its parked folder's holding path, where the refused read would count as a failure and hold back the sync point.
        @Test func theScanDoesNotReadAFileBelowAParkedFolder() async throws {
            var other = metadata("other", "other.txt", at: filesUrl + "/dst/Docs")
            other.downloaded = true
            store(metadata("dst", "dst", at: filesUrl, directory: true), metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true), other)
            write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))

            let scan = try await Enumerator(enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: log).scanMaterialisedItemsForRemoteChanges()

            #expect(!scan.hadFailure)
            #expect(!scan.deleted.map(\.ocId).contains("other"))
            #expect(manager.itemMetadata(ocId: "other")?.deleted == false)
        }

        /// Once the deletion of a parked folder is acknowledged and its row removed, a visited subfolder left at the holding path is not enumerated in the working set, where it would have no parent.
        @Test func aFolderBelowARemovedParkedFolderIsLeftOutOfTheWorkingSet() async throws {
            var child = metadata("child", "Sub", at: filesUrl + "/dst/Docs", directory: true)
            child.visitedDirectory = true
            var dst = metadata("dst", "dst", at: filesUrl, directory: true)
            dst.visitedDirectory = true
            store(dst, metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true), child)
            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            manager.removeItemMetadata(ocId: "old-docs")
            #expect(manager.itemMetadata(ocId: "child")?.isAtHoldingPath == true)
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))
            let observer = try MockEnumerationObserver(enumerator: Enumerator(enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))

            try await observer.enumerateItems()

            let identifiers = observer.items.map(\.itemIdentifier.rawValue)
            #expect(identifiers.contains("dst"))
            #expect(!identifiers.contains("child"))
        }

        /// Once the deletion of a parked folder is acknowledged and its row removed, a downloaded file left at the holding path is not reported in the working set's changes, where it would have no parent.
        @Test func aFileBelowARemovedParkedFolderIsLeftOutOfTheWorkingSetChanges() async throws {
            var other = metadata("other", "other.txt", at: filesUrl + "/dst/Docs")
            other.downloaded = true
            store(metadata("dst", "dst", at: filesUrl, directory: true), metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true), other)
            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            manager.removeItemMetadata(ocId: "old-docs")
            #expect(manager.itemMetadata(ocId: "other")?.isAtHoldingPath == true)
            let root = MockRemoteItem.rootItem(account: account)
            let destination = remoteItem("dst", "dst", at: filesUrl, directory: true)
            let newDocs = remoteItem("new-docs", "Docs", at: filesUrl + "/dst", directory: true)
            root.children = [destination]
            destination.parent = root
            destination.children = [newDocs]
            newDocs.parent = destination
            let remoteInterface = MockRemoteInterface(account: account, rootItem: root)
            let observer = try MockChangeObserver(enumerator: Enumerator(enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))

            try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: Date(timeIntervalSinceNow: -300), storeIdentity: manager.storeIdentity))

            #expect(!observer.changedItems.map(\.itemIdentifier.rawValue).contains("other"))
        }

        /// A change below a parked folder that never reaches the server, to an ignored file or a lock file's local row, is handled as anywhere else, while a new lock file, which would lock a file on the server, waits for the folder's placement.
        @Test func aLocalOnlyChangeBelowAParkedFolderGoesThrough() async throws {
            let holdingPath = filesUrl + "/dst/Docs\u{1}old-docs"
            var lockFile = metadata("lock-file", "~$Report.docx", at: filesUrl + "/dst/Docs")
            lockFile.isLockFileOfLocalOrigin = true
            lockFile.uploaded = false
            lockFile.classFile = "lock"
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("ignored", ".DS_Store", at: filesUrl + "/dst/Docs"),
                lockFile
            )
            write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let ignoredFiles = IgnoredFilesMatcher(ignoreList: [".DS_Store"], log: log)
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))
            let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
            try Data("Local content".utf8).write(to: contents)
            defer { try? FileManager.default.removeItem(at: contents) }
            let storedLockFile = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier("lock-file"), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))
            let storedIgnored = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier("ignored"), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))
            let ignoredTemplate = Item(metadata: metadata("new-ignored", ".DS_Store", at: holdingPath), parentItemIdentifier: NSFileProviderItemIdentifier("old-docs"), account: account, remoteInterface: remoteInterface, dbManager: manager)
            let lockTemplate = Item(metadata: metadata("new-lock", "~$Other.docx", at: holdingPath), parentItemIdentifier: NSFileProviderItemIdentifier("old-docs"), account: account, remoteInterface: remoteInterface, dbManager: manager)

            let (_, createIgnoredError) = await Item.create(basedOn: ignoredTemplate, contents: contents, account: account, remoteInterface: remoteInterface, ignoredFiles: ignoredFiles, progress: Progress(), dbManager: manager, log: log)
            let (_, lockWriteError) = await storedLockFile.modify(itemTarget: storedLockFile, changedFields: [.contents], contents: contents, dbManager: manager)
            let deleteIgnoredError = await storedIgnored.delete(ignoredFiles: ignoredFiles, dbManager: manager)
            let (_, createLockError) = await Item.create(basedOn: lockTemplate, contents: contents, account: account, remoteInterface: remoteInterface, progress: Progress(), dbManager: manager, log: log)

            #expect((createIgnoredError as? NSFileProviderError)?.code == .excludedFromSync)
            #expect(lockWriteError == nil)
            #expect(deleteIgnoredError == nil)
            #expect((createLockError as? NSFileProviderError)?.code == .cannotSynchronize)
            #expect(remoteInterface.uploadOperationCount == 0)
            #expect(remoteInterface.lastDeleteRemotePath == nil)
        }

        /// A save, a move out of or into, or a new item below a parked folder waits until a listing has placed the folder again, so no request goes to the holding path, while a change that sends no request goes through.
        @Test func aSaveMoveOrCreateBelowAParkedFolderWaitsForItsPlacement() async throws {
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("other", "other.txt", at: filesUrl + "/dst/Docs"),
                metadata("outside", "outside.txt", at: filesUrl + "/dst")
            )
            write([metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)
            let holdingPath = filesUrl + "/dst/Docs\u{1}old-docs"
            let remoteInterface = MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account))
            let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
            try Data("Updated content".utf8).write(to: contents)
            defer { try? FileManager.default.removeItem(at: contents) }
            let parked = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier("other"), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))
            let outside = try #require(await Item.storedItem(identifier: NSFileProviderItemIdentifier("outside"), account: account, remoteInterface: remoteInterface, dbManager: manager, log: log))
            var movedOutMetadata = parked.metadata
            movedOutMetadata.serverUrl = filesUrl + "/dst"
            let moveOutTarget = Item(metadata: movedOutMetadata, parentItemIdentifier: NSFileProviderItemIdentifier("dst"), account: account, remoteInterface: remoteInterface, dbManager: manager)
            var movedInMetadata = outside.metadata
            movedInMetadata.serverUrl = holdingPath
            let moveInTarget = Item(metadata: movedInMetadata, parentItemIdentifier: NSFileProviderItemIdentifier("old-docs"), account: account, remoteInterface: remoteInterface, dbManager: manager)
            let template = Item(metadata: metadata("new-file", "new.txt", at: holdingPath), parentItemIdentifier: NSFileProviderItemIdentifier("old-docs"), account: account, remoteInterface: remoteInterface, dbManager: manager)

            let (_, saveError) = await parked.modify(itemTarget: parked, changedFields: [.contents], contents: contents, dbManager: manager)
            let (lastUsed, lastUsedError) = await parked.modify(itemTarget: parked, changedFields: [.lastUsedDate], contents: nil, dbManager: manager)
            let (_, moveOutError) = await parked.modify(itemTarget: moveOutTarget, changedFields: [.parentItemIdentifier], contents: nil, dbManager: manager)
            let (_, moveInError) = await outside.modify(itemTarget: moveInTarget, changedFields: [.parentItemIdentifier], contents: nil, dbManager: manager)
            let (created, createError) = await Item.create(basedOn: template, contents: contents, account: account, remoteInterface: remoteInterface, progress: Progress(), dbManager: manager, log: log)

            for error in [saveError, moveOutError, moveInError, createError] {
                #expect((error as? NSFileProviderError)?.code == .cannotSynchronize)
            }
            #expect(created == nil)
            #expect(lastUsed != nil && lastUsedError == nil)
            #expect(remoteInterface.uploadOperationCount == 0)
            #expect(remoteInterface.lastMoveRemotePathSource == nil)
            #expect(manager.itemMetadata(ocId: "other")?.serverUrl == holdingPath)
            #expect(manager.itemMetadata(ocId: "outside")?.serverUrl == filesUrl + "/dst")
            #expect(manager.itemMetadata(ocId: "new-file") == nil)
        }

        /// A folder parked while live keeps its deleted children when it is placed again, as a live folder does.
        @Test func foldersThatSwappedNamesKeepTheirDeletedChildren() {
            var excluded = metadata("x", "x.txt", at: filesUrl + "/P/A")
            excluded.deleted = true
            store(metadata("p", "P", at: filesUrl, directory: true), metadata("a", "A", at: filesUrl + "/P", directory: true), excluded, metadata("b", "B", at: filesUrl + "/P", directory: true))

            write([metadata("p", "P", at: filesUrl, directory: true), metadata("b", "A", at: filesUrl + "/P", directory: true), metadata("a", "B", at: filesUrl + "/P", directory: true)], through: .depthOne)
            #expect(manager.itemMetadata(ocId: "x")?.serverUrl == filesUrl + "/P/B")
            let changes = write([metadata("a", "B", at: filesUrl + "/P", directory: true), metadata("x", "x.txt", at: filesUrl + "/P/B")], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "x")?.deleted == true)
            #expect(changes?.updated.contains { $0.ocId == "x" } == false)
        }

        /// A folder already deleted when it was parked still leaves its deleted children behind when the server lists it elsewhere, so each is written again where it is listed.
        @Test func aFolderDeletedBeforeItWasParkedGetsItsChildrenBackWhereTheyAreListed() {
            store(
                metadata("dst", "dst", at: filesUrl, directory: true),
                metadata("archive", "Archive", at: filesUrl + "/dst", directory: true),
                metadata("old-docs", "Docs", at: filesUrl + "/dst", directory: true),
                metadata("old-file", "old.txt", at: filesUrl + "/dst/Docs")
            )
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "old-docs")
            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("archive", "Archive", at: filesUrl + "/dst", directory: true), metadata("new-docs", "Docs", at: filesUrl + "/dst", directory: true)], through: .depthOne)

            write([metadata("archive", "Archive", at: filesUrl + "/dst", directory: true), metadata("old-docs", "Docs-2023", at: filesUrl + "/dst/Archive", directory: true)], through: .depthOne)
            let inFolder = write([metadata("old-docs", "Docs-2023", at: filesUrl + "/dst/Archive", directory: true), metadata("old-file", "old.txt", at: filesUrl + "/dst/Archive/Docs-2023")], through: .depthOne)

            #expect(manager.itemMetadata(ocId: "old-file")?.serverUrl == filesUrl + "/dst/Archive/Docs-2023")
            #expect(manager.itemMetadata(ocId: "old-file")?.deleted == false)
            #expect(inFolder?.updated.contains { $0.ocId == "old-file" } == true)
        }

        /// A deleted subfolder of a deleted folder stays behind with everything below it, so no moved row is left without its parent.
        @Test(arguments: Read.allCases)
        func aLiveFileBelowADeletedSubfolderStaysWithIt(read: Read) {
            var folder = metadata("folder", "Ontwerp", at: filesUrl + "/Projects", directory: true)
            folder.deleted = true
            var subfolder = metadata("subfolder", "sub", at: filesUrl + "/Projects/Ontwerp", directory: true)
            subfolder.deleted = true
            store(
                metadata("projects", "Projects", at: filesUrl, directory: true),
                metadata("archive", "Archive", at: filesUrl, directory: true),
                folder,
                metadata("file", "a.indd", at: filesUrl + "/Projects/Ontwerp"),
                subfolder,
                metadata("nested", "b.indd", at: filesUrl + "/Projects/Ontwerp/sub")
            )

            let changes = write([metadata("archive", "Archive", at: filesUrl, directory: true), metadata("folder", "Ontwerp", at: filesUrl + "/Archive", directory: true)], through: read)

            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == filesUrl + "/Archive/Ontwerp")
            #expect(manager.itemMetadata(ocId: "nested")?.serverUrl == filesUrl + "/Projects/Ontwerp/sub")
            #expect(manager.itemMetadata(ocId: "nested").flatMap { manager.parentItemIdentifierFromMetadata($0) } == NSFileProviderItemIdentifier("subfolder"))
            if read == .depthOne {
                #expect(changes?.updated.contains { $0.ocId == "nested" } == false)
            }
        }

        /// On the server `/b/T` came from `/a/T`, and its subfolder `D` was replaced by another folder of the same name.
        @Test func aSubfolderReplacedInAMovedFolderIsNotWrittenBack() {
            store(
                metadata("a", "a", at: filesUrl, directory: true),
                metadata("b", "b", at: filesUrl, directory: true),
                metadata("folder", "T", at: filesUrl + "/a", directory: true),
                metadata("old-sub", "D", at: filesUrl + "/a/T", directory: true),
                metadata("old-file", "d.txt", at: filesUrl + "/a/T/D")
            )

            let changes = write([metadata("folder", "T", at: filesUrl + "/b", directory: true), metadata("new-sub", "D", at: filesUrl + "/b/T", directory: true)], through: .depthOne)

            #expect(liveRows(at: filesUrl + "/b/T", named: "D").map(\.ocId) == ["new-sub"])
            #expect(liveRows(at: filesUrl + "/b/T/D", named: "d.txt").isEmpty)
            #expect(changes?.updated.contains { $0.ocId == "old-sub" || $0.ocId == "old-file" } == false)
        }

        /// A folder whose read returned 404 is tombstoned with its children, and when the server lists it elsewhere each child is written again where it is listed.
        @Test(arguments: Read.allCases)
        func aFolderTombstonedBeforeTheMoveGetsItsChildrenBackWhereTheyAreListed(read: Read) {
            seedFolderBelowSource()
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "folder")

            let move = write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("folder", "oud", at: filesUrl + "/dst", directory: true)], through: read)
            #expect(move?.updated.contains { $0.deleted } != true, "A deleted descendant must not be reported as an existing item.")
            let inFolder = write([
                metadata("folder", "oud", at: filesUrl + "/dst", directory: true),
                metadata("file", "a.indd", at: filesUrl + "/dst/oud"),
                metadata("subfolder", "sub", at: filesUrl + "/dst/oud", directory: true)
            ], through: read)
            let inSubfolder = write([metadata("subfolder", "sub", at: filesUrl + "/dst/oud", directory: true), metadata("nested", "b.indd", at: filesUrl + "/dst/oud/sub")], through: read)

            #expect(["folder", "file", "subfolder", "nested"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false })
            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == filesUrl + "/dst/oud")
            #expect(manager.itemMetadata(ocId: "nested")?.serverUrl == filesUrl + "/dst/oud/sub")
            if read == .depthOne {
                let updatedInFolder = Set(inFolder?.updated.map(\.ocId) ?? [])
                #expect(updatedInFolder.isSuperset(of: ["file", "subfolder"]))
                #expect(inSubfolder?.updated.contains { $0.ocId == "nested" } == true)
            }
        }

        /// A folder tombstoned after a 404 shares its path with a new folder the user created there, so when it is listed elsewhere it leaves the new folder's rows in place.
        @Test(arguments: Read.allCases)
        func aTombstonedFolderListedElsewhereLeavesTheRowsOfANewFolderAtItsPath(read: Read) {
            seedFolderBelowSource()
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "folder")
            var report = metadata("new-report", "Report.docx", at: filesUrl + "/src/Ontwerp")
            report.lockToken = "token"
            var lockFile = metadata("lock-file", "~$Report.docx", at: filesUrl + "/src/Ontwerp")
            lockFile.isLockFileOfLocalOrigin = true
            lockFile.uploaded = false
            lockFile.classFile = "lock"
            store(metadata("new-folder", "Ontwerp", at: filesUrl + "/src", directory: true), report, lockFile)

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("folder", "Ontwerp", at: filesUrl + "/dst", directory: true)], through: read)

            #expect(manager.itemMetadata(ocId: "folder")?.serverUrl == filesUrl + "/dst")
            #expect(manager.itemMetadata(ocId: "new-folder")?.serverUrl == filesUrl + "/src")
            #expect(manager.itemMetadata(ocId: "new-folder")?.deleted == false)
            #expect(manager.itemMetadata(ocId: "new-report")?.serverUrl == filesUrl + "/src/Ontwerp")
            #expect(manager.itemMetadata(ocId: "new-report")?.lockToken == "token")
            #expect(manager.itemMetadata(ocId: "lock-file")?.serverUrl == filesUrl + "/src/Ontwerp")

            write([metadata("folder", "Ontwerp", at: filesUrl + "/dst", directory: true), metadata("file", "a.indd", at: filesUrl + "/dst/Ontwerp"), metadata("subfolder", "sub", at: filesUrl + "/dst/Ontwerp", directory: true)], through: read)
            write([metadata("new-folder", "Ontwerp", at: filesUrl + "/src", directory: true), metadata("new-report", "Report.docx", at: filesUrl + "/src/Ontwerp")], through: read)

            #expect(manager.itemMetadata(ocId: "file")?.serverUrl == filesUrl + "/dst/Ontwerp")
            #expect(manager.itemMetadata(ocId: "subfolder")?.serverUrl == filesUrl + "/dst/Ontwerp")
            #expect(["file", "subfolder"].allSatisfy { manager.itemMetadata(ocId: $0)?.deleted == false }, "The moved folder's children are written again where the server lists them.")
            #expect(liveRows(at: filesUrl + "/src/Ontwerp", named: "a.indd").isEmpty, "A child of the moved folder is not brought back below the new folder.")
            #expect(manager.itemMetadata(ocId: "new-report")?.serverUrl == filesUrl + "/src/Ontwerp")
            #expect(manager.itemMetadata(ocId: "lock-file")?.serverUrl == filesUrl + "/src/Ontwerp")
        }

        /// A folder tombstoned after a 404 whose old path holds only another tombstone takes its own live rows along when it is listed elsewhere, so they keep a parent.
        @Test(arguments: Read.allCases)
        func aTombstonedFolderListedElsewhereKeepsItsLiveRowsWhenOnlyATombstoneHoldsItsPath(read: Read) {
            seedFolderBelowSource()
            var lockFile = metadata("lock-file", "~$a.indd", at: filesUrl + "/src/Ontwerp")
            lockFile.isLockFileOfLocalOrigin = true
            lockFile.uploaded = false
            lockFile.classFile = "lock"
            var older = metadata("older", "Ontwerp", at: filesUrl + "/src", directory: true)
            older.deleted = true
            store(lockFile, older)
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "folder")

            write([metadata("dst", "dst", at: filesUrl, directory: true), metadata("folder", "Ontwerp", at: filesUrl + "/dst", directory: true)], through: read)

            #expect(manager.itemMetadata(ocId: "folder")?.serverUrl == filesUrl + "/dst")
            #expect(manager.itemMetadata(ocId: "lock-file")?.serverUrl == filesUrl + "/dst/Ontwerp")
            #expect(manager.itemMetadata(ocId: "lock-file")?.deleted == false)
        }

        /// A folder that a scan read reports as moved, and that another enumeration removes before the scan ends, is not reported by the scan, which then counts as incomplete so the sync point does not move past it.
        @Test func theScanDoesNotReportAFolderRemovedDuringTheScan() async throws {
            var destination = metadata("dst", "dst", at: filesUrl, directory: true)
            destination.visitedDirectory = true
            var later = metadata("later", "later", at: filesUrl + "/src", directory: true)
            later.visitedDirectory = true
            store(metadata("src", "src", at: filesUrl, directory: true), destination, metadata("folder", "Ontwerp", at: filesUrl + "/src", directory: true), later)
            let root = MockRemoteItem.rootItem(account: account)
            let source = remoteItem("src", "src", at: filesUrl, directory: true)
            let remoteDestination = remoteItem("dst", "dst", at: filesUrl, directory: true)
            let atDestination = remoteItem("folder", "Ontwerp", at: filesUrl + "/dst", directory: true)
            let laterItem = remoteItem("later", "later", at: filesUrl + "/src", directory: true)
            root.children = [source, remoteDestination]
            source.parent = root
            source.children = [laterItem]
            laterItem.parent = source
            remoteDestination.parent = root
            remoteDestination.children = [atDestination]
            atDestination.parent = remoteDestination
            let remoteInterface = MockRemoteInterface(account: account, rootItem: root)
            remoteInterface.enumerateCallHandler = { [manager] remotePath, _, _, _, _, _, _, _ in
                if remotePath == filesUrl + "/src/later" {
                    manager.removeItemMetadata(ocId: "folder")
                }
            }

            let scan = try await Enumerator(enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: log).scanMaterialisedItemsForRemoteChanges()

            #expect(scan.hadFailure)
            #expect(!scan.updated.map(\.ocId).contains("folder"))
            #expect(!scan.deleted.map(\.ocId).contains("folder"))
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
