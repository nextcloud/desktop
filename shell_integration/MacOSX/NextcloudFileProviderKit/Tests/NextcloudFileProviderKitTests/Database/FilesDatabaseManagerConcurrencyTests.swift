//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for concurrent access: readers next to a writer, and two managers on one file.
    ///
    @Suite("Concurrency")
    struct FilesDatabaseManagerConcurrencyTests {
        let manager = DatabaseTestSuites.makeManager()

        @Test func readersAreServedWhileAWriterRuns() async throws {
            let folderUrl = DatabaseTestSuites.account.davFilesUrl + "/Big"
            var folder = DatabaseTestSuites.makeFile(ocId: "big", fileName: "Big")
            folder.directory = true
            let children = (0 ..< 2000).map { index -> SendableItemMetadata in
                var child = DatabaseTestSuites.makeFile(ocId: "child-\(index)", fileName: "child-\(index).txt", serverUrl: folderUrl)
                child.uploaded = true
                return child
            }
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "probe", fileName: "probe.txt"))

            let manager = manager
            async let write: ChangeSet? = Task.detached {
                manager.depth1ReadUpdateItemMetadatas(account: DatabaseTestSuites.account.ncKitAccount, serverUrl: folderUrl, updatedMetadatas: [folder] + children, keepExistingDownloadState: true)
            }.value

            var reads = 0
            for _ in 0 ..< 200 {
                if manager.itemMetadata(ocId: "probe") != nil {
                    reads += 1
                }
            }

            let changeSet = try #require(await write)
            #expect(changeSet.created.count == 2001)
            #expect(reads == 200)
        }

        @Test func twoManagersOnOneFileSeeEachOthersWrites() throws {
            let url = try #require(manager.databaseURL)
            let second = try FilesDatabaseManager(
                account: DatabaseTestSuites.account,
                databaseDirectory: url.deletingLastPathComponent(),
                fileProviderDomainIdentifier: NSFileProviderDomainIdentifier(url.deletingPathExtension().lastPathComponent),
                log: FileProviderLogMock()
            )

            for index in 0 ..< 50 {
                manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "first-\(index)", fileName: "first-\(index).txt"))
                second.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "second-\(index)", fileName: "second-\(index).txt"))
            }

            #expect(manager.itemMetadatas(account: DatabaseTestSuites.account.ncKitAccount).count == 100)
            #expect(second.itemMetadata(ocId: "first-49") != nil)
            #expect(manager.itemMetadata(ocId: "second-49") != nil)
        }
    }
}
