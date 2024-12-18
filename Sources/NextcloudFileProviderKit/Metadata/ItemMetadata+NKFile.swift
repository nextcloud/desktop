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
import NextcloudKit

public extension ItemMetadata {
    static func metadatasFromDirectoryReadNKFiles(_ files: [NKFile], account: Account) -> (
        directoryMetadata: ItemMetadata,
        childDirectoriesMetadatas: [ItemMetadata],
        metadatas: [ItemMetadata]
    ) {
        var directoryMetadataSet = false
        var directoryMetadata = ItemMetadata()
        var childDirectoriesMetadatas: [ItemMetadata] = []
        var metadatas: [ItemMetadata] = []

        for file in files {
            if metadatas.isEmpty, !directoryMetadataSet {
                let metadata = file.toItemMetadata()
                directoryMetadata = metadata
                directoryMetadataSet = true
            } else {
                let metadata = file.toItemMetadata()
                metadatas.append(metadata)
                if metadata.directory {
                    childDirectoriesMetadatas.append(metadata)
                }
            }
        }

        return (directoryMetadata, childDirectoriesMetadatas, metadatas)
    }
}
