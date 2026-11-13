//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for what a depth-1 directory listing does to rows which are missing from it, known at another location, or below a deleted directory.
    ///
    @Suite("Directory listings")
    struct DirectoryListingTests {
        let manager = DatabaseTestSuites.makeManager()
        let account = DatabaseTestSuites.account
        let old = Date(timeIntervalSinceReferenceDate: 1000)

        private func makeDirectory(ocId: String, fileName: String, serverUrl: String? = nil) -> SendableItemMetadata {
            var directory = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName, serverUrl: serverUrl)
            directory.directory = true
            directory.uploaded = true
            directory.syncTime = old
            return directory
        }

        private func makeChild(ocId: String, fileName: String, of directory: SendableItemMetadata) -> SendableItemMetadata {
            var child = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName, serverUrl: directory.serverUrl + "/" + directory.fileName)
            child.uploaded = true
            child.syncTime = old
            return child
        }

        @discardableResult
        private func list(_ directory: SendableItemMetadata, children: [SendableItemMetadata]) -> ChangeSet? {
            manager.depth1ReadUpdateItemMetadatas(
                account: account.ncKitAccount,
                serverUrl: directory.serverUrl + "/" + directory.fileName,
                updatedMetadatas: [directory] + children,
                keepExistingDownloadState: true
            )
        }

        @Test func aPaginatedListingTombstonesTheChildrenNoPageReturned() throws {
            let directory = makeDirectory(ocId: "dir", fileName: "Dir")
            let kept = makeChild(ocId: "kept", fileName: "kept.txt", of: directory)
            let gone = makeChild(ocId: "gone", fileName: "gone.txt", of: directory)
            var goneDirectory = makeChild(ocId: "goneDir", fileName: "GoneDir", of: directory)
            goneDirectory.directory = true
            goneDirectory.visitedDirectory = true
            var deep = makeChild(ocId: "deep", fileName: "deep.txt", of: goneDirectory)
            deep.downloaded = true
            var local = makeChild(ocId: "local", fileName: "local.txt", of: directory)
            local.uploaded = false
            var uploading = makeChild(ocId: "uploading", fileName: "uploading.txt", of: directory)
            uploading.status = Status.inUpload.rawValue
            var lockFile = makeChild(ocId: "lock", fileName: ".~lock.kept.txt#", of: directory)
            lockFile.isLockFileOfLocalOrigin = true
            for row in [directory, kept, gone, goneDirectory, deep, local, uploading, lockFile] {
                try manager.insertForTesting(row)
            }
            let listingStart = Date()
            var keptAsListed = kept
            keptAsListed.syncTime = Date() // A page's rows are converted, and stamped, after the listing started.
            manager.addItemMetadataPreservingLocalState(keptAsListed)

            let tombstoned = try #require(manager.deleteChildrenNotListed(of: directory.serverUrl + "/" + directory.fileName, account: account.ncKitAccount, since: listingStart))

            #expect(Set(tombstoned.map(\.ocId)) == ["gone", "goneDir", "deep"])
            for ocId in ["gone", "goneDir", "deep"] {
                let row = try #require(manager.itemMetadata(ocId: ocId))
                #expect(row.deleted)
                #expect(row.syncTime >= listingStart, "Stamped, so the working-set derivation finds the deletion by date.")
            }
            for ocId in ["kept", "local", "uploading", "lock"] {
                #expect(manager.itemMetadata(ocId: ocId)?.deleted == false, "\(ocId) is not the server's to drop.")
            }
            #expect(manager.itemMetadata(ocId: "dir")?.deleted == false, "The listed directory itself is not a child.")
        }

        @Test func aPaginatedListingWhichTombstonesNothingReturnsNoRows() throws {
            let directory = makeDirectory(ocId: "dir", fileName: "Dir")
            try manager.insertForTesting(directory)
            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(manager.deleteChildrenNotListed(of: directory.serverUrl + "/" + directory.fileName, account: account.ncKitAccount, since: Date()) == nil)
        }

        @Test func aMissingChildIsTombstonedOnceAndStamped() throws {
            let directory = makeDirectory(ocId: "dir", fileName: "Dir")
            let kept = makeChild(ocId: "kept", fileName: "kept.txt", of: directory)
            let gone = makeChild(ocId: "gone", fileName: "gone.txt", of: directory)
            for row in [directory, kept, gone] {
                try manager.insertForTesting(row)
            }

            let first = try #require(list(directory, children: [kept]))
            #expect(first.deleted.map(\.ocId) == ["gone"])
            let tombstone = try #require(manager.itemMetadata(ocId: "gone"))
            #expect(tombstone.deleted)
            #expect(tombstone.syncTime > old, "The deletion is found by date should its batch be lost.")

            let second = try #require(list(directory, children: [kept]))
            #expect(second.deleted.isEmpty, "A row already marked deleted was reported when it went missing.")
            #expect(manager.itemMetadata(ocId: "gone")?.syncTime == tombstone.syncTime)
        }

        @Test func deletingASubtreeCanStampEveryRow() throws {
            let directory = makeDirectory(ocId: "dir", fileName: "Dir")
            let child = makeChild(ocId: "child", fileName: "child.txt", of: directory)
            var grandchildDirectory = makeDirectory(ocId: "sub", fileName: "Sub", serverUrl: directory.serverUrl + "/Dir")
            grandchildDirectory.visitedDirectory = true
            let grandchild = makeChild(ocId: "grand", fileName: "grand.txt", of: grandchildDirectory)
            for row in [directory, child, grandchildDirectory, grandchild] {
                try manager.insertForTesting(row)
            }

            let unstamped = try #require(manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "dir"))
            #expect(Set(unstamped.map(\.ocId)) == ["dir", "child", "sub", "grand"])
            #expect(manager.itemMetadata(ocId: "grand")?.syncTime == old, "The default leaves the stamps alone.")

            let stamp = Date(timeIntervalSinceReferenceDate: 5000)
            _ = manager.deleteDirectoryAndSubdirectoriesMetadata(ocId: "dir", stampingSyncTime: stamp)
            for ocId in ["dir", "child", "sub", "grand"] {
                let row = try #require(manager.itemMetadata(ocId: ocId))
                #expect(row.deleted)
                #expect(row.syncTime == stamp)
            }
        }
    }
}
