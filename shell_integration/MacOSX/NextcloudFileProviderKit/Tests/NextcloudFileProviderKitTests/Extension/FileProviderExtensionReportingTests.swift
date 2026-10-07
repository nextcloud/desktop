//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
@testable import TestInterface

///
/// Coverage for which action replies count as handing item state to the system.
///
@Suite("Extension reply marking")
struct FileProviderExtensionReportingTests {
    let manager = DatabaseTestSuites.makeManager()
    let remoteInterface = MockRemoteInterface(account: DatabaseTestSuites.account)

    private func makeItem(_ metadata: SendableItemMetadata) -> Item {
        Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: DatabaseTestSuites.account,
            remoteInterface: remoteInterface,
            dbManager: manager,
            displayFileActions: false,
            remoteSupportsTrash: false,
            log: FileProviderLogMock()
        )
    }

    private func storedRow() -> SendableItemMetadata {
        let row = DatabaseTestSuites.makeFile(ocId: "created", fileName: "created.txt")
        manager.addItemMetadata(row)
        return row
    }

    @Test func aSuccessfulReplyIsMarked() {
        let row = storedRow()

        FileProviderExtension.markReported(makeItem(row), error: nil, dbManager: manager)

        #expect(manager.reportedSyncTimeForTesting(ocId: row.ocId) == row.syncTime)
    }

    @Test func aReplyWithAnErrorIsNotMarked() {
        let row = storedRow()

        FileProviderExtension.markReported(makeItem(row), error: NSFileProviderError(.serverUnreachable), dbManager: manager)

        #expect(manager.reportedSyncTimeForTesting(ocId: row.ocId) == nil)
    }

    @Test func aTemplateWhichIsNotAnItemIsNotMarked() {
        let row = storedRow()
        let template = MockFileProviderItem(identifier: .init(row.ocId), filename: row.fileName, isUploaded: false)

        FileProviderExtension.markReported(template, error: nil, dbManager: manager)

        #expect(manager.reportedSyncTimeForTesting(ocId: row.ocId) == nil)
    }

    @Test func theSynthesisedContainersAreNotMarked() throws {
        let root = Item.rootContainer(account: DatabaseTestSuites.account, remoteInterface: remoteInterface, dbManager: manager, remoteSupportsTrash: true, log: FileProviderLogMock())
        try manager.insertForTesting(root.metadata)

        FileProviderExtension.markReported(root, error: nil, dbManager: manager)

        #expect(manager.reportedSyncTimeForTesting(ocId: root.itemIdentifier.rawValue) == nil)
    }

    @Test func withoutADatabaseNothingHappens() {
        FileProviderExtension.markReported(makeItem(storedRow()), error: nil, dbManager: nil)

        #expect(manager.reportedSyncTimeForTesting(ocId: "created") == nil)
    }
}
