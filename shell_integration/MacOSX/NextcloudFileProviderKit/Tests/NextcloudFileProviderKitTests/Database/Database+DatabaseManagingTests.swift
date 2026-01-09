//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

@Suite("Database Tests")
struct DatabaseManagingTests {
    @Test("Retrieval by File Provider Item Identifier")
    func getItemByIdentifier() async throws {
        let database = Database(log: FileProviderLogMock())
        try await database.insertItem()
        let item = database.getItem(by: <#T##NSFileProviderItemIdentifier#>)
        <#code#>
    }

    @Test("Insertion")
    func insertItem() async throws {
        let database = Database(log: FileProviderLogMock())
        try await database.insertItem()
        <#code#>
    }
}
