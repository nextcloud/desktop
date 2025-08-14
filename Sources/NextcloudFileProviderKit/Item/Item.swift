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

    public let dbManager: FilesDatabaseManager
    public let metadata: SendableItemMetadata
    public let parentItemIdentifier: NSFileProviderItemIdentifier
    public let account: Account
    public let remoteInterface: RemoteInterface

    private let remoteSupportsTrash: Bool

    public var itemIdentifier: NSFileProviderItemIdentifier {
        NSFileProviderItemIdentifier(metadata.ocId)
    }

    public var capabilities: NSFileProviderItemCapabilities {
        var capabilities: NSFileProviderItemCapabilities = []
        let permissions = metadata.permissions.uppercased()
        if permissions.contains("G"), metadata.directory { // Readable
            capabilities.insert(.allowsContentEnumerating)
        } else if permissions.contains("G") {
            capabilities.insert(.allowsReading)
        }
        if !metadata.lock {
            if permissions.contains("D") { // Deletable
                capabilities.insert(.allowsDeleting)
            }
            if remoteSupportsTrash, !isLockFileName(filename) {
                capabilities.insert(.allowsTrashing)
            }
            if permissions.contains("W"), !metadata.directory { // Updateable (file)
                capabilities.insert(.allowsWriting)
            }
            if permissions.contains("NV") { // Updateable, renameable, moveable
                capabilities.formUnion([.allowsRenaming, .allowsReparenting])

                if metadata.directory {
                    capabilities.insert(.allowsAddingSubItems)
                }
            }
        }
        // .allowsEvicting deprecated on macOS 13.0+, use contentPolicy instead
        if #unavailable(macOS 13.0), !metadata.keepDownloaded {
            capabilities.insert(.allowsEvicting)
        }
#if os(macOS)
        if #available(macOS 11.3, *) {
            capabilities.insert(.allowsExcludingFromSync)
        }
#endif
        return capabilities
    }

    public var itemVersion: NSFileProviderItemVersion {
        NSFileProviderItemVersion(
            contentVersion: metadata.etag.data(using: .utf8)!,
            metadataVersion: metadata.etag.data(using: .utf8)!)
    }

    public var filename: String {
        metadata.isTrashed && !metadata.trashbinFileName.isEmpty ?
            metadata.trashbinFileName : !metadata.fileName.isEmpty ?
                metadata.fileName : "unnamed file"
    }

    public var contentType: UTType {
        if itemIdentifier == .rootContainer || (metadata.contentType.isEmpty && metadata.directory)
        {
            return .folder
        } else if metadata.contentType == "httpd/unix-directory", metadata.directory {
            let filenameComponents = filename.components(separatedBy: ".")
            if filenameComponents.count > 1, let ext = filenameComponents.last {
                return UTType(filenameExtension: ext, conformingTo: .directory) ?? .folder
            }
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
        metadata.directory || metadata.downloaded
    }

    public var isDownloading: Bool {
        metadata.isDownload
    }

    public var downloadingError: Error? {
        if metadata.status == Status.downloadError.rawValue {
            return FileProviderItemTransferError.downloadError
        }
        return nil
    }

    public var isUploaded: Bool {
        metadata.uploaded
    }

    public var isUploading: Bool {
        metadata.isUpload
    }

    public var uploadingError: Error? {
        if metadata.status == Status.uploadError.rawValue {
            FileProviderItemTransferError.uploadError
        } else {
            nil
        }
    }

    public var isShared: Bool {
        false // !metadata.shareType.isEmpty // Interim solution to counteract Finder misleadingly displaying shared items with an iCloud branded banner.
    }

    public var isSharedByCurrentUser: Bool {
        false // isShared && metadata.ownerId == account.id // Interim solution to counteract Finder misleadingly displaying shared items with an iCloud branded banner.
    }

    public var ownerNameComponents: PersonNameComponents? {
        guard isShared, !isSharedByCurrentUser else { return nil }
        let formatter = PersonNameComponentsFormatter()
        return formatter.personNameComponents(from: metadata.ownerDisplayName)
    }

    public var childItemCount: NSNumber? {
        if metadata.directory {
            NSNumber(integerLiteral: dbManager.childItemCount(directoryMetadata: metadata))
        } else {
            nil
        }
    }

    public var fileSystemFlags: NSFileProviderFileSystemFlags {
        if metadata.lock,
           (metadata.lockOwnerType != 0 || metadata.lockOwner != account.username),
           metadata.lockTimeOut ?? Date() > Date()
        {
            return [.userReadable]
        }
        return [.userReadable, .userWritable]
    }

    public var userInfo: [AnyHashable : Any]? {
        var userInfoDict = [AnyHashable : Any]()
        if metadata.lock {
            // Can be used to display lock/unlock context menu entries for FPUIActions
            // Note that only files, not folders, should be lockable/unlockable
            userInfoDict["locked"] = metadata.lock
        }
        if #available(macOS 13.0, iOS 16.0, visionOS 1.0, *) {
            userInfoDict["displayKeepDownloaded"] = !metadata.keepDownloaded
            userInfoDict["displayAllowAutoEvicting"] = metadata.keepDownloaded
            userInfoDict["displayEvict"] = metadata.downloaded && !metadata.keepDownloaded
        } else {
            userInfoDict["displayEvict"] = metadata.downloaded
        }
        // https://docs.nextcloud.com/server/latest/developer_manual/client_apis/WebDAV/basic.html
        if metadata.permissions.uppercased().contains("R"), // Shareable
           ![.rootContainer, .trashContainer].contains(itemIdentifier)
        {
            userInfoDict["displayShare"] = true
        }
        return userInfoDict
    }

    @available(macOS 13.0, iOS 16.0, visionOS 1.0, *)
    public var contentPolicy: NSFileProviderContentPolicy {
        #if os(macOS)
        if metadata.keepDownloaded {
            return .downloadEagerlyAndKeepDownloaded // Unavailable in iOS.
        }
        #endif

        return .inherited
    }

    public var keepDownloaded: Bool {
        guard #available(macOS 13.0, iOS 16.0, visionOS 1.0, *) else { return false }
        return metadata.keepDownloaded
    }

    public static func rootContainer(
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        remoteSupportsTrash: Bool
    ) -> Item {
        let metadata = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.rootContainer.rawValue,
            account: account.ncKitAccount,
            classFile: NKCommon.TypeClassFile.directory.rawValue,
            contentType: "", // Placeholder as not set in original code
            creationDate: Date(), // Default as not set in original code
            directory: true,
            e2eEncrypted: false, // Default as not set in original code
            etag: "", // Placeholder as not set in original code
            fileId: "", // Placeholder as not set in original code
            fileName: "/",
            fileNameView: "/",
            hasPreview: false, // Default as not set in original code
            iconName: "", // Placeholder as not set in original code
            mountType: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            path: "", // Placeholder as not set in original code
            serverUrl: account.davFilesUrl,
            size: 0, // Default as not set in original code
            uploaded: true,
            urlBase: "", // Placeholder as not set in original code
            user: "", // Placeholder as not set in original code
            userId: "" // Placeholder as not set in original code

        )
        return Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: remoteSupportsTrash
        )
    }

    public static func trashContainer(
        remoteInterface: RemoteInterface,
        account: Account,
        dbManager: FilesDatabaseManager,
        remoteSupportsTrash: Bool
    ) -> Item {
        let metadata = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.trashContainer.rawValue,
            account: account.ncKitAccount,
            classFile: NKCommon.TypeClassFile.directory.rawValue,
            contentType: "", // Placeholder as not set in original code
            creationDate: Date(), // Default as not set in original code
            directory: true,
            e2eEncrypted: false, // Default as not set in original code
            etag: "", // Placeholder as not set in original code
            fileId: "", // Placeholder as not set in original code
            fileName: "Trash",
            fileNameView: "Trash",
            hasPreview: false, // Default as not set in original code
            iconName: "", // Placeholder as not set in original code
            mountType: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            path: "", // Placeholder as not set in original code
            serverUrl: account.trashUrl,
            size: 0, // Default as not set in original code
            uploaded: true,
            urlBase: "", // Placeholder as not set in original code
            user: "", // Placeholder as not set in original code
            userId: "" // Placeholder as not set in original code
        )
        return Item(
            metadata: metadata,
            parentItemIdentifier: .trashContainer,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: remoteSupportsTrash
        )
    }

    static let logger = Logger(subsystem: Logger.subsystem, category: "item")

    public required init(
        metadata: SendableItemMetadata,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        remoteSupportsTrash: Bool
    ) {
        self.metadata = metadata
        self.parentItemIdentifier = parentItemIdentifier
        self.account = account
        self.remoteInterface = remoteInterface
        self.dbManager = dbManager
        self.remoteSupportsTrash = remoteSupportsTrash
        super.init()
    }

    public static func storedItem(
        identifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) async -> Item? {
        // resolve the given identifier to a record in the model

        let remoteSupportsTrash = await remoteInterface.supportsTrash(account: account)

        guard identifier != .rootContainer else {
            return Item.rootContainer(
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: remoteSupportsTrash
            )
        }
        guard identifier != .trashContainer else {
            return Item.trashContainer(
                remoteInterface: remoteInterface,
                account: account,
                dbManager: dbManager,
                remoteSupportsTrash: remoteSupportsTrash
            )
        }

        guard let metadata = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier) else {
            return nil
        }

        var parentItemIdentifier: NSFileProviderItemIdentifier?
        if metadata.isTrashed {
            parentItemIdentifier = .trashContainer
        } else {
            parentItemIdentifier = await dbManager.parentItemIdentifierWithRemoteFallback(
                fromMetadata: metadata,
                remoteInterface: remoteInterface,
                account: account
            )
        }
        guard let parentItemIdentifier else { return nil }

        return Item(
            metadata: metadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: remoteSupportsTrash
        )
    }

    public func localUrlForContents(domain: NSFileProviderDomain) async -> URL? {
        guard isDownloaded else {
            Self.logger.error(
                """
                Unable to get local URL for item contents.
                    filename: \(self.filename, privacy: .public)
                    Item is not materialised.
                """
            )
            return nil
        }

        guard let manager = NSFileProviderManager(for: domain),
              let fileUrl = try? await manager.getUserVisibleURL(for: itemIdentifier)
        else {
            Self.logger.error(
                """
                Unable to get manager or user visible url for item.
                    filename: \(self.filename, privacy: .public)
                    Cannot provide local URL for contents.
                """
            )
            return nil
        }
        
        let fm = FileManager.default
        let tempLocation = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let coordinator = NSFileCoordinator()
        var readData: Data?
        coordinator.coordinate(readingItemAt: fileUrl, options: [], error: nil) { readURL in
            readData = try? Data(contentsOf: readURL)
        }
        guard let readData else { return nil }
        do {
            try readData.write(to: tempLocation)
        } catch let error {
            Self.logger.error(
                """
                Unable to write file item contents \(self.filename, privacy: .public) to temp url.
                    error: \(error.localizedDescription, privacy: .public)
                    Cannot provide local URL for contents.
                """
            )
        }
        return tempLocation
    }
}
