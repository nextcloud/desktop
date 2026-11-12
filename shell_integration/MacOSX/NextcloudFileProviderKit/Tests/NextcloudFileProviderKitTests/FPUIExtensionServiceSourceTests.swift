//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

struct FPUIExtensionServiceSourceTests {
    enum ItemLocation: CaseIterable {
        case files
        case filesRoot
        case trash
    }

    @Test(arguments: ItemLocation.allCases)
    func itemServerPathReturnsOnlyPathsUnderTheFilesRoot(location: ItemLocation) async throws {
        let account = DatabaseTestSuites.account
        let domain = NSFileProviderDomain(identifier: .init(UUID().uuidString), displayName: "Item path test")
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let manager = try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain.identifier,
            log: FileProviderLogMock()
        )
        let provider = FileProviderExtension(domain: domain)
        defer { provider.invalidate() }
        try #require(provider.publishDatabaseManager(manager, generation: provider.setupGeneration))

        let identifier: NSFileProviderItemIdentifier = location == .filesRoot ? .rootContainer : .init("report")
        let parentURL: String
        let fileName: String
        let expectedPath: String?
        switch location {
            case .files:
                parentURL = account.davFilesUrl + "/Team"
                fileName = "Report #1?.txt"
                expectedPath = "/Team/Report #1?.txt"
            case .filesRoot:
                parentURL = account.davFilesUrl
                fileName = "/"
                expectedPath = "/"
            case .trash:
                parentURL = account.trashUrl
                fileName = "Report.txt.d123"
                expectedPath = nil
        }

        var item = DatabaseTestSuites.makeFile(
            ocId: identifier.rawValue,
            fileName: fileName,
            serverUrl: parentURL,
            account: account
        )
        item.directory = location == .filesRoot
        manager.addItemMetadata(item)
        try #require(manager.itemMetadata(identifier) != nil)

        let path = await FPUIExtensionServiceSource(fpExtension: provider).itemServerPath(identifier: identifier)
        #expect(path.map { $0 as String } == expectedPath)
    }
}
