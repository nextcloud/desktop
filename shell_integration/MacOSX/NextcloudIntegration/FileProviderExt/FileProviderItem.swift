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
import NextcloudKit
import UniformTypeIdentifiers

class FileProviderItem: NSObject, NSFileProviderItem {
    enum FileProviderItemTransferError: Error {
        case downloadError
        case uploadError
    }

    let metadata: NextcloudItemMetadataTable
    let parentItemIdentifier: NSFileProviderItemIdentifier
    let ncKit: NextcloudKit

    var itemIdentifier: NSFileProviderItemIdentifier {
        NSFileProviderItemIdentifier(metadata.ocId)
    }

    var capabilities: NSFileProviderItemCapabilities {
        guard !metadata.directory else {
            if #available(macOS 13.0, *) {
                // .allowsEvicting deprecated on macOS 13.0+, use contentPolicy instead
                return [
                    .allowsAddingSubItems,
                    .allowsContentEnumerating,
                    .allowsReading,
                    .allowsDeleting,
                    .allowsRenaming
                ]
            } else {
                return [
                    .allowsAddingSubItems,
                    .allowsContentEnumerating,
                    .allowsReading,
                    .allowsDeleting,
                    .allowsRenaming,
                    .allowsEvicting
                ]
            }
        }
        guard !metadata.lock else {
            return [.allowsReading]
        }
        return [
            .allowsWriting,
            .allowsReading,
            .allowsDeleting,
            .allowsRenaming,
            .allowsReparenting,
            .allowsEvicting
        ]
    }

    var itemVersion: NSFileProviderItemVersion {
        NSFileProviderItemVersion(
            contentVersion: metadata.etag.data(using: .utf8)!,
            metadataVersion: metadata.etag.data(using: .utf8)!)
    }

    var filename: String {
        metadata.fileNameView
    }

    var contentType: UTType {
        if itemIdentifier == .rootContainer || metadata.directory {
            return .folder
        }

        let internalType = ncKit.nkCommonInstance.getInternalType(
            fileName: metadata.fileNameView,
            mimeType: "",
            directory: metadata.directory)
        return UTType(filenameExtension: internalType.ext) ?? .content
    }

    var documentSize: NSNumber? {
        NSNumber(value: metadata.size)
    }

    var creationDate: Date? {
        metadata.creationDate as Date
    }

    var lastUsedDate: Date? {
        metadata.date as Date
    }

    var contentModificationDate: Date? {
        metadata.date as Date
    }

    var isDownloaded: Bool {
        metadata.directory
            || NextcloudFilesDatabaseManager.shared.localFileMetadataFromOcId(metadata.ocId) != nil
    }

    var isDownloading: Bool {
        metadata.status == NextcloudItemMetadataTable.Status.downloading.rawValue
    }

    var downloadingError: Error? {
        if metadata.status == NextcloudItemMetadataTable.Status.downloadError.rawValue {
            return FileProviderItemTransferError.downloadError
        }
        return nil
    }

    var isUploaded: Bool {
        NextcloudFilesDatabaseManager.shared.localFileMetadataFromOcId(metadata.ocId) != nil
    }

    var isUploading: Bool {
        metadata.status == NextcloudItemMetadataTable.Status.uploading.rawValue
    }

    var uploadingError: Error? {
        if metadata.status == NextcloudItemMetadataTable.Status.uploadError.rawValue {
            FileProviderItemTransferError.uploadError
        } else {
            nil
        }
    }

    var childItemCount: NSNumber? {
        if metadata.directory {
            NSNumber(
                integerLiteral: NextcloudFilesDatabaseManager.shared.childItemsForDirectory(
                    metadata
                ).count)
        } else {
            nil
        }
    }

    @available(macOSApplicationExtension 13.0, *)
    var contentPolicy: NSFileProviderContentPolicy {
        .downloadLazily
    }

    required init(
        metadata: NextcloudItemMetadataTable,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        ncKit: NextcloudKit
    ) {
        self.metadata = metadata
        self.parentItemIdentifier = parentItemIdentifier
        self.ncKit = ncKit
        super.init()
    }
}
