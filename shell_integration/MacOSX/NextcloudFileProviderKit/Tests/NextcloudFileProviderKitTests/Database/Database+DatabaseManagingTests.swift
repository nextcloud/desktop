//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

@Suite("Database Tests")
struct DatabaseManagingTests {
    @Test("Database Item Insert and Fetch")
    func insertAndFetchItem() async throws {
        let database = Database(log: FileProviderLogMock())
        try await database.insertItem()
        <#code#>
    }

    @Test("Root Container Mapping")
    func mapRootContainer() async throws {
        let database = Database(log: FileProviderLogMock())
        <#code#>
    }

    @Test("Trash Container Mapping")
    func mapTrashContainer() async throws {
        let database = Database(log: FileProviderLogMock())
        <#code#>
    }
}
