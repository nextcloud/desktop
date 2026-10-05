//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for what the manager answers when the database cannot be read: lookups which guard a server-side action must fail closed.
    ///
    @Suite("Read failures")
    struct FilesDatabaseManagerFailureTests {
        let manager = DatabaseTestSuites.makeManager()

        @Test func theExclusionLookupThrowsInsteadOfAnsweringNotExcluded() throws {
            manager.markItemAsExcludedFromSync(ocId: "excluded")
            #expect(try manager.excludedFromSyncMarkerExists(ocId: "excluded"))

            try manager.breakTableForTesting(ExcludedFromSyncItemRecord.databaseTableName)

            #expect(throws: (any Error).self) {
                try manager.excludedFromSyncMarkerExists(ocId: "excluded")
            }
            #expect(manager.isItemExcludedFromSync(ocId: "excluded") == false, "The non-throwing form keeps answering false.")
        }

        @Test func chunksAreAssumedToRemainWhenTheLookupFails() throws {
            manager.addRemoteFileChunks([RemoteFileChunk(fileName: "1", size: 1, remoteChunkStoreFolderName: "u")])
            #expect(manager.hasRemoteFileChunks(uploadId: "other") == false)

            try manager.breakTableForTesting(RemoteFileChunkRecord.databaseTableName)

            #expect(manager.hasRemoteFileChunks(uploadId: "other"), "A failed lookup must not discard local chunks.")
        }

        @Test func pendingWorkingSetChangesReportFailureAsNil() throws {
            var row = DatabaseTestSuites.makeFile(ocId: "changed", fileName: "changed.txt")
            row.downloaded = true
            manager.addItemMetadata(row)
            #expect(manager.pendingWorkingSetChanges(since: Date(timeIntervalSince1970: 0))?.updated.count == 1)

            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(manager.pendingWorkingSetChanges(since: Date(timeIntervalSince1970: 0)) == nil)
            #expect(manager.itemMetadata(ocId: "changed") == nil)
            #expect(manager.itemMetadatas(account: DatabaseTestSuites.account.ncKitAccount).isEmpty)
        }
    }
}
