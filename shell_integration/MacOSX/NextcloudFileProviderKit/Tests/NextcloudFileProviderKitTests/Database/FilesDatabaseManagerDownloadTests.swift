//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

extension DatabaseTestSuites {
    @Suite("Downloads")
    struct FilesDatabaseManagerDownloadTests {
        private func expectError(_ expected: any Error, performing operation: () throws -> Void) {
            do {
                try operation()
                Issue.record("Expected an error.")
            } catch {
                let expected = expected as NSError
                let actual = error as NSError
                #expect(actual.domain == expected.domain)
                #expect(actual.code == expected.code)
            }
        }

        private func makeDirectoryDownloadOperation() throws -> DownloadOperation {
            let log = FileProviderLogMock()
            let operation = DownloadOperation(ocId: UUID().uuidString, cancellation: NetworkOperationCancellation(log: log), log: log)
            var metadata = SendableItemMetadata(ocId: operation.ocId, fileName: operation.ocId, account: DatabaseTestSuites.account)
            metadata.directory = true
            manager.addItemMetadata(metadata)
            _ = try #require(manager.beginDownload(operation))
            return operation
        }

        private func makeDirectoryDownloadOperations() throws -> (root: DownloadOperation, children: [DownloadOperation]) {
            let log = FileProviderLogMock()
            let root = DownloadOperation(ocId: UUID().uuidString, cancellation: NetworkOperationCancellation(log: log), log: log)
            let children = [root.childOperation(for: UUID().uuidString), root.childOperation(for: UUID().uuidString)]
            for (index, operation) in ([root] + children).enumerated() {
                var metadata = SendableItemMetadata(ocId: operation.ocId, fileName: operation.ocId, account: DatabaseTestSuites.account)
                metadata.directory = index != 1
                metadata.serverUrl = index == 0 ? DatabaseTestSuites.account.davFilesUrl : DatabaseTestSuites.account.davFilesUrl + "/" + root.ocId
                metadata.keepDownloaded = true
                metadata.visitedDirectory = true
                metadata.favorite = true
                metadata.lockToken = "local-lock"
                metadata.fileProviderContentVersion = "local-version"
                manager.addItemMetadata(metadata)
                let started = try #require(manager.beginDownload(operation))
                #expect(started.status == Status.downloading.rawValue)
                #expect(!started.downloaded)
            }
            return (root, children)
        }

        let manager = DatabaseTestSuites.makeManager()

        @Test func depth1ReadPreservesUnchangedDeletedChild() throws {
            let account = DatabaseTestSuites.account
            var parent = SendableItemMetadata(ocId: "restored-child-parent", fileName: "folder", account: account)
            parent.directory = true
            parent.uploaded = true
            parent.visitedDirectory = true
            manager.addItemMetadata(parent)

            var child = SendableItemMetadata(ocId: "restored-child", fileName: "file.txt", account: account)
            child.serverUrl = parent.remotePath()
            child.uploaded = true
            child.downloaded = true
            child.keepDownloaded = true
            child.visitedDirectory = true
            child.lockToken = "local-lock-token"
            child.fileProviderContentVersion = "local-content-version"
            manager.addItemMetadata(child)

            let deletion = try #require(manager.depth1ReadUpdateItemMetadatas(
                account: account.ncKitAccount,
                serverUrl: parent.remotePath(),
                updatedMetadatas: [parent],
                keepExistingDownloadState: true
            ))
            #expect(deletion.deleted.map(\.ocId) == [child.ocId])
            #expect(try #require(manager.itemMetadata(ocId: child.ocId)).deleted)

            var received = child
            received.downloaded = false
            received.keepDownloaded = false
            received.visitedDirectory = false
            received.lockToken = nil
            received.fileProviderContentVersion = ""
            let read = try #require(manager.depth1ReadUpdateItemMetadatas(
                account: account.ncKitAccount,
                serverUrl: parent.remotePath(),
                updatedMetadatas: [parent, received],
                keepExistingDownloadState: true
            ))
            #expect(read.updated.isEmpty)
            #expect(read.created.isEmpty)
            #expect(read.deleted.isEmpty)
            let stored = try #require(manager.itemMetadata(ocId: child.ocId))
            #expect(stored.deleted)
            #expect(stored.downloaded)
            #expect(stored.keepDownloaded)
            #expect(stored.visitedDirectory)
            #expect(stored.lockToken == child.lockToken)
            #expect(stored.fileProviderContentVersion == child.fileProviderContentVersion)

            let repeated = try #require(manager.depth1ReadUpdateItemMetadatas(
                account: account.ncKitAccount,
                serverUrl: parent.remotePath(),
                updatedMetadatas: [parent, received],
                keepExistingDownloadState: true
            ))
            #expect(repeated.updated.isEmpty)
        }

        @Test func depth1ReadPreservesUnchangedDeletedReadTarget() throws {
            var directory = SendableItemMetadata(ocId: "restored-read-target", fileName: "folder", account: DatabaseTestSuites.account)
            directory.directory = true
            directory.uploaded = true
            directory.downloaded = true
            directory.keepDownloaded = true
            directory.visitedDirectory = true
            directory.fileProviderContentVersion = "local-content-version"
            manager.addItemMetadata(directory)
            #expect(manager.deleteItemMetadata(ocId: directory.ocId))

            var received = directory
            received.downloaded = false
            received.keepDownloaded = false
            received.fileProviderContentVersion = ""
            let changes = try #require(manager.depth1ReadUpdateItemMetadatas(
                account: DatabaseTestSuites.account.ncKitAccount,
                serverUrl: directory.remotePath(),
                updatedMetadatas: [received],
                keepExistingDownloadState: true
            ))
            #expect(changes.updated.isEmpty)
            #expect(changes.created.isEmpty)
            #expect(changes.deleted.isEmpty)
            let stored = try #require(manager.itemMetadata(ocId: directory.ocId))
            #expect(stored.deleted)
            #expect(stored.downloaded)
            #expect(stored.keepDownloaded)
            #expect(stored.visitedDirectory)
            #expect(stored.fileProviderContentVersion == directory.fileProviderContentVersion)
        }

        @Test func depth1ReadDoesNotRestoreDeletedItemsInTransit() throws {
            for directory in [false, true] {
                for status in [Status.downloading, .uploading] {
                    var metadata = SendableItemMetadata(ocId: "deleted-transit", fileName: "item", account: DatabaseTestSuites.account)
                    metadata.directory = directory
                    metadata.uploaded = true
                    metadata.deleted = true
                    metadata.status = status.rawValue
                    metadata.sessionError = "existing-error"
                    manager.addItemMetadata(metadata)

                    var received = metadata
                    received.deleted = false
                    received.status = Status.normal.rawValue
                    received.sessionError = ""
                    let changes = try #require(manager.depth1ReadUpdateItemMetadatas(
                        account: DatabaseTestSuites.account.ncKitAccount,
                        serverUrl: metadata.serverUrl,
                        updatedMetadatas: [received],
                        keepExistingDownloadState: true
                    ))
                    #expect(changes.updated.isEmpty)
                    let stored = try #require(manager.itemMetadata(ocId: metadata.ocId))
                    #expect(stored.deleted)
                    #expect(stored.status == status.rawValue)
                    #expect(stored.sessionError == metadata.sessionError)
                }
            }
        }

        @Test func directoryDownloadSelectionUsesServerListingAndPreservesDatabaseState() throws {
            for directory in [false, true] {
                for deleted in [false, true] {
                    for status in [Status.normal, .inDownload, .downloading, .downloadError, .inUpload, .uploading, .uploadError] {
                        var metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
                        metadata.directory = directory
                        metadata.uploaded = true
                        metadata.downloaded = true
                        metadata.keepDownloaded = true
                        metadata.visitedDirectory = true
                        metadata.favorite = true
                        metadata.lockToken = "local-lock"
                        metadata.fileProviderContentVersion = "local-version"
                        metadata.sessionError = "existing-error"
                        metadata.status = status.rawValue
                        metadata.deleted = deleted
                        manager.addItemMetadata(metadata)
                        let before = try #require(manager.itemMetadata(ocId: metadata.ocId))
                        var received = metadata
                        received.serverUrl += "/renamed-parent"
                        received.fileName += "-renamed"
                        received.fileNameView = received.fileName
                        received.etag = "server-version"
                        received.size = 128
                        received.deleted = false
                        received.downloaded = false
                        received.keepDownloaded = false
                        received.visitedDirectory = false
                        received.fileProviderContentVersion = nil
                        received.favorite = false
                        received.status = Status.normal.rawValue
                        received.sessionError = ""
                        received.lockToken = nil

                        let selected = try manager.directoryDownloadMetadatas([received])

                        #expect(selected == (deleted ? [] : [received]))
                        #expect(manager.itemMetadata(ocId: metadata.ocId) == before)
                    }
                }
            }
        }

        @Test func directoryDownloadSelectionRejectsMissingMetadata() {
            let metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: "missing-child.txt", account: DatabaseTestSuites.account)

            expectError(NSFileProviderError(.cannotSynchronize)) { try manager.directoryDownloadMetadatas([metadata]) }
            #expect(manager.itemMetadata(ocId: metadata.ocId) == nil)
        }

        @Test func directoryDownloadSelectionThrowsWhenTheDatabaseCannotBeRead() throws {
            let metadata = DatabaseTestSuites.makeFile(ocId: "listed-child", fileName: "child.txt")
            try manager.insertForTesting(metadata)
            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(throws: (any Error).self) {
                try manager.directoryDownloadMetadatas([metadata])
            }
        }

        @Test func packageDownloadRestorationPreservesLiveMetadata() throws {
            let operation = try makeDirectoryDownloadOperation()
            defer { try? manager.finishDownload(operation, status: .normal, downloaded: false) }
            for directory in [false, true] {
                for status in [Status.normal, .inDownload, .downloading, .downloadError, .inUpload, .uploading, .uploadError] {
                    var metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
                    metadata.directory = directory
                    metadata.uploaded = true
                    metadata.downloaded = true
                    metadata.keepDownloaded = true
                    metadata.visitedDirectory = true
                    metadata.favorite = true
                    metadata.lockToken = "local-lock"
                    metadata.fileProviderContentVersion = "local-version"
                    metadata.sessionError = "existing-error"
                    metadata.status = status.rawValue
                    manager.addItemMetadata(metadata)
                    let before = try #require(manager.itemMetadata(ocId: metadata.ocId))
                    var received = metadata
                    received.fileName += "-server"
                    received.downloaded = false
                    received.keepDownloaded = false
                    received.favorite = false
                    received.status = Status.normal.rawValue
                    received.sessionError = ""
                    received.lockToken = nil

                    try manager.restorePackageDownloadMetadatas([received], for: operation)

                    #expect(manager.itemMetadata(ocId: metadata.ocId) == before)
                }
            }
        }

        @Test func packageDownloadRestorationPreservesDeletedMembersLocalState() throws {
            let operation = try makeDirectoryDownloadOperation()
            defer { try? manager.finishDownload(operation, status: .normal, downloaded: false) }
            for directory in [false, true] {
                for status in [Status.normal, .downloadError] {
                    var metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
                    metadata.directory = directory
                    metadata.status = status.rawValue
                    metadata.uploaded = true
                    metadata.downloaded = true
                    metadata.keepDownloaded = true
                    metadata.visitedDirectory = true
                    metadata.favorite = true
                    metadata.lockToken = "local-lock"
                    metadata.fileProviderContentVersion = "local-version"
                    metadata.sessionError = "existing-error"
                    metadata.deleted = true
                    manager.addItemMetadata(metadata)
                    var expected = try #require(manager.itemMetadata(ocId: metadata.ocId))
                    expected.deleted = false
                    var received = metadata
                    received.deleted = false
                    received.downloaded = false
                    received.keepDownloaded = false
                    received.favorite = false
                    received.sessionError = ""
                    received.lockToken = nil

                    try manager.restorePackageDownloadMetadatas([received], for: operation)

                    #expect(manager.itemMetadata(ocId: metadata.ocId) == expected)
                }
            }
        }

        @Test func packageDownloadRestorationInsertsMissingMetadata() throws {
            let operation = try makeDirectoryDownloadOperation()
            defer { try? manager.finishDownload(operation, status: .normal, downloaded: false) }
            let metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)

            try manager.restorePackageDownloadMetadatas([metadata], for: operation)

            #expect(manager.itemMetadata(ocId: metadata.ocId) == metadata)
        }

        @Test func packageDownloadRestorationRejectsDeletedTransferStateAtomically() throws {
            let operation = try makeDirectoryDownloadOperation()
            defer { try? manager.finishDownload(operation, status: .normal, downloaded: false) }
            for status in [Status.inDownload, .downloading, .inUpload, .uploading, .uploadError] {
                let missing = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
                var restorable = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
                restorable.deleted = true
                manager.addItemMetadata(restorable)
                let beforeRestorable = try #require(manager.itemMetadata(ocId: restorable.ocId))
                var blocked = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
                blocked.deleted = true
                blocked.status = status.rawValue
                blocked.sessionError = "existing-error"
                manager.addItemMetadata(blocked)
                let beforeBlocked = try #require(manager.itemMetadata(ocId: blocked.ocId))
                var receivedRestorable = restorable
                receivedRestorable.deleted = false
                var receivedBlocked = blocked
                receivedBlocked.deleted = false
                receivedBlocked.status = Status.normal.rawValue
                receivedBlocked.sessionError = ""

                expectError(NSFileProviderError(.cannotSynchronize)) { try manager.restorePackageDownloadMetadatas([receivedBlocked], for: operation) }
                expectError(NSFileProviderError(.cannotSynchronize)) { try manager.restorePackageDownloadMetadatas([missing, receivedRestorable, receivedBlocked], for: operation) }

                #expect(manager.itemMetadata(ocId: missing.ocId) == nil)
                #expect(manager.itemMetadata(ocId: restorable.ocId) == beforeRestorable)
                #expect(manager.itemMetadata(ocId: blocked.ocId) == beforeBlocked)
            }
        }

        @Test func packageDownloadRestorationRejectsDeletedParent() throws {
            let operation = try makeDirectoryDownloadOperation()
            defer { try? manager.finishDownload(operation, status: .normal, downloaded: false) }
            var metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
            metadata.deleted = true
            manager.addItemMetadata(metadata)
            #expect(manager.deleteItemMetadata(ocId: operation.ocId))

            expectError(NSFileProviderError(.cannotSynchronize)) { try manager.restorePackageDownloadMetadatas([metadata], for: operation) }

            #expect(try #require(manager.itemMetadata(ocId: metadata.ocId)).deleted)
        }

        @Test func packageDownloadRestorationRejectsSupersededParent() throws {
            let operation = try makeDirectoryDownloadOperation()
            let log = FileProviderLogMock()
            let newer = DownloadOperation(ocId: operation.ocId, cancellation: NetworkOperationCancellation(log: log), log: log)
            _ = try #require(manager.beginDownload(newer))
            defer { try? manager.finishDownload(newer, status: .normal, downloaded: false) }
            let metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)

            expectError(NSFileProviderError(.cannotSynchronize)) { try manager.restorePackageDownloadMetadatas([metadata], for: operation) }

            #expect(manager.itemMetadata(ocId: metadata.ocId) == nil)
        }

        @Test func packageDownloadRestorationRejectsCancelledParent() throws {
            let operation = try makeDirectoryDownloadOperation()
            defer { try? manager.finishDownload(operation, status: .normal, downloaded: false) }
            let metadata = SendableItemMetadata(ocId: UUID().uuidString, fileName: UUID().uuidString, account: DatabaseTestSuites.account)
            operation.cancellation.cancel()

            expectError(CocoaError(.userCancelled)) { try manager.restorePackageDownloadMetadatas([metadata], for: operation) }

            #expect(manager.itemMetadata(ocId: metadata.ocId) == nil)
        }

        @Test func packageDownloadRestorationRejectsOwnedDeletedChild() throws {
            let (operation, children) = try makeDirectoryDownloadOperations()
            defer {
                for download in [operation] + children {
                    try? manager.finishDownload(download, status: .normal, downloaded: false)
                }
            }
            var metadata = try #require(manager.itemMetadata(ocId: children[0].ocId))
            metadata.deleted = true
            metadata.status = Status.normal.rawValue
            manager.addItemMetadata(metadata)

            expectError(NSFileProviderError(.cannotSynchronize)) { try manager.restorePackageDownloadMetadatas([metadata], for: operation) }

            #expect(manager.itemMetadata(ocId: metadata.ocId) == metadata)
        }

        @Test func directoryDownloadCompletionPublishesAllItemsAndPreservesLocalState() throws {
            let (root, children) = try makeDirectoryDownloadOperations()
            let operations = [root] + children
            defer {
                for operation in operations {
                    try? manager.finishDownload(operation, status: .normal, downloaded: false)
                }
            }
            let before = try operations.map { try #require(manager.itemMetadata(ocId: $0.ocId)) }

            try manager.finishDirectoryDownload(root, children: children)

            for var expected in before {
                expected.status = Status.normal.rawValue
                expected.downloaded = true
                expected.uploaded = true
                expected.sessionError = ""
                #expect(manager.itemMetadata(ocId: expected.ocId) == expected)
            }
        }

        @Test func directoryDownloadCompletionRetainsOwnershipAfterWriteFailure() throws {
            let (root, children) = try makeDirectoryDownloadOperations()
            let operations = [root] + children
            defer {
                for operation in operations {
                    try? manager.finishDownload(operation, status: .normal, downloaded: false)
                }
            }
            let snapshots = try operations.map { try #require(manager.itemMetadata(ocId: $0.ocId)) }
            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(throws: (any Error).self) {
                try manager.finishDirectoryDownload(root, children: children)
            }

            manager.recreateTablesForTesting()
            for metadata in snapshots {
                try manager.insertForTesting(metadata)
            }
            try manager.finishDirectoryDownload(root, children: children)

            for var expected in snapshots {
                expected.status = Status.normal.rawValue
                expected.downloaded = true
                expected.uploaded = true
                expected.sessionError = ""
                #expect(manager.itemMetadata(ocId: expected.ocId) == expected)
            }
        }

        @Test func directoryDownloadCompletionRejectsMissingChildWithoutPublishingOtherItems() throws {
            for removeRow in [false, true] {
                let (root, children) = try makeDirectoryDownloadOperations()
                let operations = [root] + children
                defer {
                    for operation in operations {
                        try? manager.finishDownload(operation, status: .normal, downloaded: false)
                    }
                }
                let missingChild = try #require(children.last)
                if removeRow {
                    manager.removeItemMetadata(ocId: missingChild.ocId)
                } else {
                    #expect(manager.deleteItemMetadata(ocId: missingChild.ocId))
                }
                let before = operations.compactMap { manager.itemMetadata(ocId: $0.ocId) }

                expectError(NSFileProviderError(.cannotSynchronize)) { try manager.finishDirectoryDownload(root, children: children) }

                for metadata in before {
                    #expect(manager.itemMetadata(ocId: metadata.ocId) == metadata)
                }
                for operation in [root] + Array(children.dropLast()) {
                    try manager.finishDownload(operation, status: .normal, downloaded: false)
                    let discarded = try #require(manager.itemMetadata(ocId: operation.ocId))
                    #expect(discarded.status == Status.normal.rawValue)
                    #expect(!discarded.downloaded)
                }
                if removeRow {
                    #expect(manager.itemMetadata(ocId: missingChild.ocId) == nil)
                } else {
                    #expect(try #require(manager.itemMetadata(ocId: missingChild.ocId)).deleted)
                }
            }
        }

        @Test func directoryDownloadCompletionPreservesSupersedingChildState() throws {
            for newerCompleted in [false, true] {
                let (root, children) = try makeDirectoryDownloadOperations()
                let child = children[0]
                let log = FileProviderLogMock()
                let newer = DownloadOperation(ocId: child.ocId, cancellation: NetworkOperationCancellation(log: log), log: log)
                defer {
                    for operation in [root] + children + [newer] {
                        try? manager.finishDownload(operation, status: .normal, downloaded: false)
                    }
                }
                _ = try #require(manager.beginDownload(newer))
                if newerCompleted {
                    try manager.finishDownload(newer, status: .normal, downloaded: true)
                }
                let expectedNewer = try #require(manager.itemMetadata(ocId: child.ocId))
                let otherOperations = [root] + Array(children.dropFirst())
                let before = try otherOperations.map { try #require(manager.itemMetadata(ocId: $0.ocId)) }

                expectError(NSFileProviderError(.cannotSynchronize)) { try manager.finishDirectoryDownload(root, children: children) }

                for metadata in before {
                    #expect(manager.itemMetadata(ocId: metadata.ocId) == metadata)
                }
                for operation in [root] + children {
                    try? manager.finishDownload(operation, status: .normal, downloaded: false)
                }
                #expect(manager.itemMetadata(ocId: child.ocId) == expectedNewer)
                if !newerCompleted {
                    try manager.finishDownload(newer, status: .normal, downloaded: true)
                    #expect(try #require(manager.itemMetadata(ocId: child.ocId)).downloaded)
                }
            }
        }

        @Test func reopenedManagerPreservesNewerDownloadState() throws {
            try manager.removeAllRowsForTesting()
            let operation = try makeDirectoryDownloadOperation()
            let url = try #require(manager.databaseURL)
            let reopened = try FilesDatabaseManager(
                account: DatabaseTestSuites.account,
                databaseDirectory: url.deletingLastPathComponent(),
                fileProviderDomainIdentifier: NSFileProviderDomainIdentifier(url.deletingPathExtension().lastPathComponent),
                log: FileProviderLogMock(),
                defaults: DatabaseTestSuites.defaults
            )
            let log = FileProviderLogMock()
            let newer = DownloadOperation(ocId: operation.ocId, cancellation: NetworkOperationCancellation(log: log), log: log)
            defer {
                try? manager.finishDownload(operation, status: .normal, downloaded: false)
                try? reopened.finishDownload(newer, status: .normal, downloaded: false)
            }
            _ = try #require(reopened.beginDownload(newer))
            try reopened.finishDownload(newer, status: .normal, downloaded: true)
            let expected = try #require(reopened.itemMetadata(ocId: operation.ocId))

            expectError(NSFileProviderError(.cannotSynchronize)) {
                try manager.finishDownload(operation, status: .normal, downloaded: false)
            }

            #expect(manager.itemMetadata(ocId: operation.ocId) == expected)
            #expect(operation.cancellation.isCancelled)
        }

        @Test func directoryDownloadCompletionReportsMissingRequestedRoot() throws {
            let (root, children) = try makeDirectoryDownloadOperations()
            defer {
                for operation in [root] + children {
                    try? manager.finishDownload(operation, status: .normal, downloaded: false)
                }
            }
            manager.removeItemMetadata(ocId: root.ocId)
            let before = try children.map { try #require(manager.itemMetadata(ocId: $0.ocId)) }

            expectError(NSFileProviderError(.noSuchItem)) { try manager.finishDirectoryDownload(root, children: children) }

            #expect(manager.itemMetadata(ocId: root.ocId) == nil)
            for metadata in before {
                #expect(manager.itemMetadata(ocId: metadata.ocId) == metadata)
            }
        }
    }
}
