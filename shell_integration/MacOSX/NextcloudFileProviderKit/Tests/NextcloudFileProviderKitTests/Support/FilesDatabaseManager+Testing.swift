//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import RealmSwift

///
/// Direct access to stored rows for tests.
///
/// This is the only place in the test target which touches the storage engine. Tests seed and inspect rows through these methods so that an engine change touches one file.
///
extension FilesDatabaseManager {
    ///
    /// Store a row exactly as given, bypassing duplicate eviction and the inheritance of local flags.
    ///
    /// The normalized location keys are derived from the raw location unless overridden, which builds rows whose keys drifted.
    ///
    func insertForTesting(_ metadata: SendableItemMetadata, normalizedServerUrl: String? = nil, normalizedFileName: String? = nil) throws {
        let database = ncDatabase()
        let row = RealmItemMetadata(value: metadata)

        if let normalizedServerUrl {
            row.normalizedServerUrl = normalizedServerUrl
        }

        if let normalizedFileName {
            row.normalizedFileName = normalizedFileName
        }

        try database.write {
            database.add(row, update: .all)
        }
    }

    ///
    /// The stored normalized location keys of a row.
    ///
    func normalizedLocationForTesting(ocId: String) -> (serverUrl: String, fileName: String)? {
        guard let row = ncDatabase().object(ofType: RealmItemMetadata.self, forPrimaryKey: ocId) else {
            return nil
        }

        return (row.normalizedServerUrl, row.normalizedFileName)
    }

    ///
    /// Every stored item row, ordered by identifier.
    ///
    func allItemMetadatasForTesting() -> [SendableItemMetadata] {
        itemMetadatas.sorted(byKeyPath: "ocId").toUnmanagedResults()
    }

    ///
    /// Remove every row of every table.
    ///
    func removeAllRowsForTesting() throws {
        let database = ncDatabase()

        try database.write {
            database.deleteAll()
        }
    }
}

extension SendableItemMetadata {
    ///
    /// A row carrying only the storage defaults: empty strings, `false`, zero and the current date.
    ///
    /// Use it where a test used to build a bare database object and set a handful of fields, so the untouched fields keep the values the engine would have given them.
    ///
    static func rawRow(ocId: String) -> SendableItemMetadata {
        SendableItemMetadata(
            ocId: ocId,
            account: "",
            classFile: "",
            contentType: "",
            creationDate: Date(),
            directory: false,
            e2eEncrypted: false,
            etag: "",
            fileId: "",
            fileName: "",
            fileNameView: "",
            ownerId: "",
            ownerDisplayName: "",
            path: "",
            permissions: "",
            serverUrl: "",
            size: 0,
            urlBase: "",
            user: "",
            userId: ""
        )
    }
}
