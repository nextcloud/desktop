/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import FileProvider
import UniformTypeIdentifiers
import NextcloudKit

class FileProviderItem: NSObject, NSFileProviderItem {

    enum FileProviderItemTransferError: Error {
        case downloadError
        case uploadError
    }

    let metadata: NextcloudItemMetadataTable
    let parentItemIdentifier: NSFileProviderItemIdentifier
    let ncKit: NextcloudKit
    
    var itemIdentifier: NSFileProviderItemIdentifier {
        return NSFileProviderItemIdentifier(metadata.ocId)
    }
    
    var capabilities: NSFileProviderItemCapabilities {
        guard !metadata.directory else {
            return [ .allowsAddingSubItems,
                     .allowsContentEnumerating,
                     .allowsReading,
                     .allowsDeleting,
                     .allowsRenaming ]
        }
        guard !metadata.lock else {
            return [ .allowsReading ]
        }
        return [ .allowsWriting,
                 .allowsReading,
                 .allowsDeleting,
                 .allowsRenaming,
                 .allowsReparenting ]
    }
    
    var itemVersion: NSFileProviderItemVersion {
        NSFileProviderItemVersion(contentVersion: metadata.etag.data(using: .utf8)!,
                                  metadataVersion: metadata.etag.data(using: .utf8)!)
    }
    
    var filename: String {
        return metadata.fileNameView
    }
    
    var contentType: UTType {
        if self.itemIdentifier == .rootContainer || metadata.directory {
            return .folder
        }

        let internalType = ncKit.nkCommonInstance.getInternalType(fileName: metadata.fileNameView,
                                                                  mimeType: "",
                                                                  directory: metadata.directory)
        return UTType(filenameExtension: internalType.ext) ?? .content
    }

    var documentSize: NSNumber? {
        return NSNumber(value: metadata.size)
    }

    var creationDate: Date? {
        return metadata.creationDate as Date
    }

    var lastUsedDate: Date? {
        return metadata.date as Date
    }

    var contentModificationDate: Date? {
        return metadata.date as Date
    }

    var isDownloaded: Bool {
        return metadata.directory || NextcloudFilesDatabaseManager.shared.localFileMetadataFromOcId(metadata.ocId) != nil
    }

    var isDownloading: Bool {
        return metadata.status == NextcloudItemMetadataTable.Status.downloading.rawValue
    }

    var downloadingError: Error? {
        if metadata.status == NextcloudItemMetadataTable.Status.downloadError.rawValue {
            return FileProviderItemTransferError.downloadError
        }
        return nil
    }

    var isUploaded: Bool {
        return NextcloudFilesDatabaseManager.shared.localFileMetadataFromOcId(metadata.ocId) != nil
    }

    var isUploading: Bool {
        return metadata.status == NextcloudItemMetadataTable.Status.uploading.rawValue
    }

    var uploadingError: Error? {
        if metadata.status == NextcloudItemMetadataTable.Status.uploadError.rawValue {
            return FileProviderItemTransferError.uploadError
        } else {
            return nil
        }
    }

    var childItemCount: NSNumber? {
        if metadata.directory {
            return NSNumber(integerLiteral: NextcloudFilesDatabaseManager.shared.childItemsForDirectory(metadata).count)
        } else {
            return nil
        }
    }

    required init(metadata: NextcloudItemMetadataTable, parentItemIdentifier: NSFileProviderItemIdentifier, ncKit: NextcloudKit) {
        self.metadata = metadata
        self.parentItemIdentifier = parentItemIdentifier
        self.ncKit = ncKit
        super.init()
    }
}
