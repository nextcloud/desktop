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

    lazy var dbManager: FilesDatabaseManager = .shared

    public let metadata: ItemMetadata
    public let parentItemIdentifier: NSFileProviderItemIdentifier
    public let remoteInterface: RemoteInterface

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
        metadata.fileNameView.isEmpty ? "unnamed file" : metadata.fileNameView
    }

    public var contentType: UTType {
        if itemIdentifier == .rootContainer ||
           (metadata.contentType.isEmpty && metadata.directory)
        {
            return .folder
        } else if !metadata.contentType.isEmpty, let type = UTType(metadata.contentType) {
            return type
        }

        let filenameExtension = filename.components(separatedBy: ".").last ?? ""
        return UTType(filenameExtension: filenameExtension) ?? .content
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
        metadata.directory || dbManager.localFileMetadataFromOcId(metadata.ocId) != nil
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
        dbManager.localFileMetadataFromOcId(metadata.ocId) != nil
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
            NSNumber(integerLiteral: dbManager.childItemsForDirectory(metadata).count)
        } else {
            nil
        }
    }

    public var fileSystemFlags: NSFileProviderFileSystemFlags {
        if metadata.lock,
           metadata.lockOwner != metadata.userId,
           metadata.lockTimeOut ?? Date() > Date()
        {
            return [.userReadable]
        }
        return [.userReadable, .userWritable]
    }

    @available(macOS 13.0, *)
    public var contentPolicy: NSFileProviderContentPolicy {
        #if os(macOS)
        .downloadLazily
        #else
        .downloadLazilyAndEvictOnRemoteUpdate
        #endif
    }

    public static func rootContainer(remoteInterface: RemoteInterface) -> Item {
        let metadata = ItemMetadata()
        metadata.account = remoteInterface.account.ncKitAccount
        metadata.directory = true
        metadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
        metadata.fileName = "/"
        metadata.fileNameView = "/"
        metadata.serverUrl = remoteInterface.account.davFilesUrl
        metadata.classFile = NKCommon.TypeClassFile.directory.rawValue
        return Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )
    }

    static let logger = Logger(subsystem: Logger.subsystem, category: "item")

    public required init(
        metadata: ItemMetadata,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        remoteInterface: RemoteInterface
    ) {
        self.metadata = ItemMetadata(value: metadata) // Safeguard against active items
        self.parentItemIdentifier = parentItemIdentifier
        self.remoteInterface = remoteInterface
        super.init()
    }

    public static func storedItem(
        identifier: NSFileProviderItemIdentifier,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager = .shared
    ) -> Item? {
        // resolve the given identifier to a record in the model
        Self.logger.debug(
            "Received request for item with identifier: \(identifier.rawValue, privacy: .public)"
        )

        guard identifier != .rootContainer else {
            return Item.rootContainer(remoteInterface: remoteInterface)
        }

        guard let metadata = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier),
              let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(metadata)
        else { return nil }

        return Item(
            metadata: metadata,
            parentItemIdentifier: parentItemIdentifier,
            remoteInterface: remoteInterface
        )
    }
}
