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

extension FilesDatabaseManager {
    public func localFileMetadataFromOcId(_ ocId: String) -> LocalFileMetadata? {
        if let metadata = ncDatabase().objects(LocalFileMetadata.self).filter(
            "ocId == %@", ocId
        ).first {
            return LocalFileMetadata(value: metadata)
        }

        return nil
    }

    public func addLocalFileMetadataFromItemMetadata(_ itemMetadata: ItemMetadata) {
        let database = ncDatabase()

        do {
            try database.write {
                let newLocalFileMetadata = LocalFileMetadata()

                newLocalFileMetadata.ocId = itemMetadata.ocId
                newLocalFileMetadata.fileName = itemMetadata.fileName
                newLocalFileMetadata.account = itemMetadata.account
                newLocalFileMetadata.etag = itemMetadata.etag
                newLocalFileMetadata.exifDate = Date()
                newLocalFileMetadata.exifLatitude = "-1"
                newLocalFileMetadata.exifLongitude = "-1"

                database.add(newLocalFileMetadata, update: .all)
                Self.logger.debug(
                    "Added local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: .public)"
                )
            }
        } catch {
            Self.logger.error(
                "Could not add local file metadata from item metadata. ocID: \(itemMetadata.ocId, privacy: .public), etag: \(itemMetadata.etag, privacy: .public), fileName: \(itemMetadata.fileName, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    public func deleteLocalFileMetadata(ocId: String) {
        let database = ncDatabase()

        do {
            try database.write {
                let results = database.objects(LocalFileMetadata.self).filter(
                    "ocId == %@", ocId)
                database.delete(results)
            }
        } catch {
            Self.logger.error(
                "Could not delete local file metadata with ocId: \(ocId, privacy: .public), received error: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    private func sortedLocalFileMetadatas(
        _ metadatas: Results<LocalFileMetadata>
    ) -> [LocalFileMetadata] {
        let sortedMetadatas = metadatas.sorted(byKeyPath: "fileName", ascending: true)
        return Array(sortedMetadatas.map { LocalFileMetadata(value: $0) })
    }

    public func localFileMetadatas(account: String) -> [LocalFileMetadata] {
        let results = ncDatabase().objects(LocalFileMetadata.self).filter(
            "account == %@", account)
        return sortedLocalFileMetadatas(results)
    }

    public func localFileItemMetadatas(account: String) -> [ItemMetadata] {
        let localFileMetadatas = localFileMetadatas(account: account)
        let localFileMetadatasOcIds = Array(localFileMetadatas.map(\.ocId))

        var itemMetadatas: [ItemMetadata] = []

        for ocId in localFileMetadatasOcIds {
            guard let itemMetadata = itemMetadataFromOcId(ocId) else {
                Self.logger.error(
                    "Could not find matching item metadata for local file metadata with ocId: \(ocId, privacy: .public) with request from account: \(account)"
                )
                continue
            }

            itemMetadatas.append(ItemMetadata(value: itemMetadata))
        }

        return itemMetadatas
    }
}
