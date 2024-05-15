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
import OSLog

public class Item: NSObject, NSFileProviderItem {
    public enum FileProviderItemTransferError: Error {
        case downloadError
        case uploadError
    }

    public let metadata: ItemMetadata
    public let parentItemIdentifier: NSFileProviderItemIdentifier
    public let ncKit: NextcloudKit

    public var itemIdentifier: NSFileProviderItemIdentifier {
        NSFileProviderItemIdentifier(metadata.ocId)
    }

    public var capabilities: NSFileProviderItemCapabilities {
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

    public var itemVersion: NSFileProviderItemVersion {
        NSFileProviderItemVersion(
            contentVersion: metadata.etag.data(using: .utf8)!,
            metadataVersion: metadata.etag.data(using: .utf8)!)
    }

    public var filename: String {
        metadata.fileNameView
    }

    public var contentType: UTType {
        if itemIdentifier == .rootContainer || metadata.directory {
            return .folder
        }

        let internalType = ncKit.nkCommonInstance.getInternalType(
            fileName: metadata.fileNameView,
            mimeType: "",
            directory: metadata.directory)
        return UTType(filenameExtension: internalType.ext) ?? .content
    }

    public var documentSize: NSNumber? {
        NSNumber(value: metadata.size)
    }

    public var creationDate: Date? {
        metadata.creationDate as Date
    }

    public var lastUsedDate: Date? {
        metadata.date as Date
    }

    public var contentModificationDate: Date? {
        metadata.date as Date
    }

    public var isDownloaded: Bool {
        metadata.directory
            || FilesDatabaseManager.shared.localFileMetadataFromOcId(metadata.ocId) != nil
    }

    public var isDownloading: Bool {
        metadata.status == ItemMetadata.Status.downloading.rawValue
    }

    public var downloadingError: Error? {
        if metadata.status == ItemMetadata.Status.downloadError.rawValue {
            return FileProviderItemTransferError.downloadError
        }
        return nil
    }

    public var isUploaded: Bool {
        FilesDatabaseManager.shared.localFileMetadataFromOcId(metadata.ocId) != nil
    }

    public var isUploading: Bool {
        metadata.status == ItemMetadata.Status.uploading.rawValue
    }

    public var uploadingError: Error? {
        if metadata.status == ItemMetadata.Status.uploadError.rawValue {
            FileProviderItemTransferError.uploadError
        } else {
            nil
        }
    }

    public var childItemCount: NSNumber? {
        if metadata.directory {
            NSNumber(
                integerLiteral: FilesDatabaseManager.shared.childItemsForDirectory(
                    metadata
                ).count)
        } else {
            nil
        }
    }

    @available(macOS 13.0, *)
    public var contentPolicy: NSFileProviderContentPolicy {
        .downloadLazily
    }

    public static func rootContainer(ncKit: NextcloudKit) -> Item {
        let metadata = ItemMetadata()
        metadata.account = ncKit.nkCommonInstance.account
        metadata.directory = true
        metadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
        metadata.serverUrl = ncKit.nkCommonInstance.urlBase
        metadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        return Item(metadata: metadata, parentItemIdentifier: .rootContainer, ncKit: ncKit)
    }

    static let logger = Logger(subsystem: Logger.subsystem, category: "item")

    public required init(
        metadata: ItemMetadata,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        ncKit: NextcloudKit
    ) {
        self.metadata = metadata
        self.parentItemIdentifier = parentItemIdentifier
        self.ncKit = ncKit
        super.init()
    }

    public static func storedItem(
        identifier: NSFileProviderItemIdentifier,
        usingKit ncKit: NextcloudKit
    ) -> Item? {
        // resolve the given identifier to a record in the model
        Self.logger.debug(
            "Received request for item with identifier: \(identifier.rawValue, privacy: .public)"
        )

        guard identifier != .rootContainer else { return Item.rootContainer(ncKit: ncKit) }

        let dbManager = FilesDatabaseManager.shared

        guard let metadata = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier),
              let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(metadata)
        else { return nil }

        return Item(metadata: metadata, parentItemIdentifier: parentItemIdentifier, ncKit: ncKit)
    }
}
