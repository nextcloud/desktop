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
    // TODO: Convert to async/await
    static func metadatasFromDirectoryReadNKFiles(
        _ files: [NKFile],
        account: String,
        completionHandler: @escaping (
            _ directoryMetadata: ItemMetadata,
            _ childDirectoriesMetadatas: [ItemMetadata],
            _ metadatas: [ItemMetadata]
        ) -> Void
    ) {
        var directoryMetadataSet = false
        var directoryMetadata = ItemMetadata()
        var childDirectoriesMetadatas: [ItemMetadata] = []
        var metadatas: [ItemMetadata] = []

        let conversionQueue = DispatchQueue(
            label: "nkFileToMetadataConversionQueue", 
            qos: .userInitiated,
            attributes: .concurrent)
        // appendQueue is a serial queue, not concurrent
        let appendQueue = DispatchQueue(label: "metadataAppendQueue", qos: .userInitiated)
        let dispatchGroup = DispatchGroup()

        for file in files {
            if metadatas.isEmpty, !directoryMetadataSet {
                let metadata = file.toItemMetadata()
                directoryMetadata = metadata
                directoryMetadataSet = true
            } else {
                conversionQueue.async(group: dispatchGroup) {
                    let metadata = file.toItemMetadata()

                    appendQueue.async(group: dispatchGroup) {
                        metadatas.append(metadata)
                        if metadata.directory {
                            childDirectoriesMetadatas.append(metadata)
                        }
                    }
                }
            }
        }

        dispatchGroup.notify(queue: DispatchQueue.main) {
            completionHandler(directoryMetadata, childDirectoriesMetadatas, metadatas)
        }
    }
}
