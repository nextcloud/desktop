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
    }
}
