/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import Foundation
import OSLog
import RealmSwift

extension NextcloudFilesDatabaseManager {
    func localFileMetadataFromOcId(_ ocId: String) -> NextcloudLocalFileMetadataTable? {
        if let metadata = ncDatabase().objects(NextcloudLocalFileMetadataTable.self).filter(
            "ocId == %@", ocId
        ).first {
            return NextcloudLocalFileMetadataTable(value: metadata)
        }

        return nil
    }

    func addLocalFileMetadataFromItemMetadata(_ itemMetadata: NextcloudItemMetadataTable) {
        let database = ncDatabase()

        do {
            try database.write {
                let newLocalFileMetadata = NextcloudLocalFileMetadataTable()

                newLocalFileMetadata.ocId = itemMetadata.ocId
                newLocalFileMetadata.fileName = itemMetadata.fileName
                newLocalFileMetadata.account = itemMetadata.account
                newLocalFileMetadata.etag = itemMetadata.etag
                newLocalFileMetadata.exifDate = Date()
                newLocalFileMetadata.exifLatitude = "-1"
                newLocalFileMetadata.exifLongitude = "-1"

                database.add(newLocalFileMetadata, update: .all)
                Logger.ncFilesDatabase.debug(
                    "Added local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: .public)"
                )
            }
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not add local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    func deleteLocalFileMetadata(ocId: String) {
        let database = ncDatabase()

        do {
            try database.write {
                let results = database.objects(NextcloudLocalFileMetadataTable.self).filter(
                    "ocId == %@", ocId)
                database.delete(results)
            }
        } catch {
            Logger.ncFilesDatabase.error(
                "Could not delete local file metadata with ocId: \(ocId, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    private func sortedLocalFileMetadatas(_ metadatas: Results<NextcloudLocalFileMetadataTable>)
        -> [NextcloudLocalFileMetadataTable]
    {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "fileName", ascending: true)
        return Array(sortedMetadatas.map { NextcloudLocalFileMetadataTable(value: $0) })
    }

    func localFileMetadatas(account: String) -> [NextcloudLocalFileMetadataTable] {
        let results = ncDatabase().objects(NextcloudLocalFileMetadataTable.self).filter(
            "account == %@", account)
        return sortedLocalFileMetadatas(results)
    }

    func localFileItemMetadatas(account: String) -> [NextcloudItemMetadataTable] {
        let localFileMetadatas = localFileMetadatas(account: account)
        let localFileMetadatasOcIds = Array(localFileMetadatas.map(\.ocId))

        var itemMetadatas: [NextcloudItemMetadataTable] = []

        for ocId in localFileMetadatasOcIds {
            guard let itemMetadata = itemMetadataFromOcId(ocId) else {
                Logger.ncFilesDatabase.error(
                    "Could not find matching item metadata for local file metadata with ocId: \(ocId, privacy: .public) with request from account: \(account)"
                )
                continue
            }

            itemMetadatas.append(NextcloudItemMetadataTable(value: itemMetadata))
        }

        return itemMetadatas
    }
}
