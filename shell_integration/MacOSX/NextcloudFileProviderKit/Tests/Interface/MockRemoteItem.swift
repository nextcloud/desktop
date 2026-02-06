//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import UniformTypeIdentifiers

public let trashedItemIdSuffix = "-trashed-mri"

public class MockRemoteItem: Equatable {
    public var parent: MockRemoteItem?
    public var children: [MockRemoteItem] = []

    public var identifier: String
    public var versionIdentifier: String
    public var name: String
    public var remotePath: String
    public let directory: Bool
    public let creationDate: Date
    public var modificationDate: Date
    public var data: Data?
    public var locked: Bool
    public var lockOwner: String
    public var lockTimeOut: Date?
    public var size: Int64 {
        Int64(data?.count ?? 0)
    }

    public var account: String
    public var username: String
    public var userId: String
    public var serverUrl: String
    public var trashbinOriginalLocation: String?

    public static func == (lhs: MockRemoteItem, rhs: MockRemoteItem) -> Bool {
        lhs.parent == rhs.parent &&
            lhs.children == rhs.children &&
            lhs.identifier == rhs.identifier &&
            lhs.versionIdentifier == rhs.versionIdentifier &&
            lhs.name == rhs.name &&
            lhs.directory == rhs.directory &&
            lhs.locked == rhs.locked &&
            lhs.lockOwner == rhs.lockOwner &&
            lhs.lockTimeOut == rhs.lockTimeOut &&
            lhs.data == rhs.data &&
            lhs.size == rhs.size &&
            lhs.creationDate == rhs.creationDate &&
            lhs.modificationDate == rhs.modificationDate &&
            lhs.account == rhs.account &&
            lhs.username == rhs.username &&
            lhs.userId == rhs.userId &&
            lhs.serverUrl == rhs.serverUrl &&
            lhs.trashbinOriginalLocation == rhs.trashbinOriginalLocation
    }

    public static func rootItem(account: Account) -> MockRemoteItem {
        MockRemoteItem(
            identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
            versionIdentifier: "root",
            name: NextcloudKit.shared.nkCommonInstance.rootFileName,
            remotePath: account.davFilesUrl,
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
    }

    public static func rootTrashItem(account: Account) -> MockRemoteItem {
        MockRemoteItem(
            identifier: NSFileProviderItemIdentifier.trashContainer.rawValue,
            versionIdentifier: "root",
            name: "",
            remotePath: account.trashUrl,
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
    }

    public init(
        identifier: String,
        versionIdentifier: String = "0",
        name: String,
        remotePath: String,
        directory: Bool = false,
        creationDate: Date = Date(),
        modificationDate: Date = Date(),
        data: Data? = nil,
        locked: Bool = false,
        lockOwner: String = "",
        lockTimeOut: Date? = nil,
        account: String,
        username: String,
        userId: String,
        serverUrl: String,
        trashbinOriginalLocation: String? = nil
    ) {
        self.identifier = identifier
        self.versionIdentifier = versionIdentifier
        self.name = name
        self.remotePath = remotePath
        self.directory = directory
        self.creationDate = creationDate
        self.modificationDate = modificationDate
        self.data = data
        self.locked = locked
        self.lockOwner = lockOwner
        self.lockTimeOut = lockTimeOut
        self.account = account
        self.username = username
        self.userId = userId
        self.serverUrl = serverUrl
        self.trashbinOriginalLocation = trashbinOriginalLocation
    }

    public func toNKFile() -> NKFile {
        let isRoot = identifier == NSFileProviderItemIdentifier.rootContainer.rawValue
        var file = NKFile()
        file.fileName = isRoot
            ? NextcloudKit.shared.nkCommonInstance.rootFileName
            : trashbinOriginalLocation?.split(separator: "/").last?.toString() ?? name
        file.size = size
        file.date = creationDate
        file.directory = isRoot ? false : directory
        file.etag = versionIdentifier
        file.ocId = identifier
        file.fileId = identifier.replacingOccurrences(of: trashedItemIdSuffix, with: "")
        file.serverUrl = isRoot ? "\(serverUrl)/remote.php/dav/files/\(userId)" : parent?.remotePath ?? serverUrl
        file.account = account
        file.user = username
        file.userId = userId
        file.urlBase = serverUrl
        file.lock = locked
        file.lockOwner = lockOwner
        file.lockTimeOut = lockTimeOut
        file.trashbinFileName = name
        file.trashbinOriginalLocation = trashbinOriginalLocation ?? ""
        return file
    }

    public func toNKTrash() -> NKTrash {
        var trashItem = NKTrash()
        trashItem.ocId = identifier
        trashItem.fileId = identifier.replacingOccurrences(of: trashedItemIdSuffix, with: "")
        trashItem.fileName = name
        trashItem.directory = directory
        trashItem.trashbinOriginalLocation = trashbinOriginalLocation ?? ""
        trashItem.trashbinFileName = trashbinOriginalLocation?.split(separator: "/").last?.toString() ?? name
        trashItem.size = size
        trashItem.filePath = (parent?.remotePath ?? "") + "/" + name
        return trashItem
    }

    public func toItemMetadata(account: Account) -> SendableItemMetadata {
        let originalFileName = trashbinOriginalLocation?.split(separator: "/").last?.toString()
        let fileName = originalFileName ?? name
        let serverUrlTrimCount = name.count

        var trimmedServerUrl = remotePath
        if identifier != NSFileProviderItemIdentifier.rootContainer.rawValue {
            trimmedServerUrl.removeSubrange(
                remotePath.index(
                    remotePath.endIndex, offsetBy: -(serverUrlTrimCount + 1) // Remove trailing slash
                ) ..< remotePath.endIndex
            )
        }

        return SendableItemMetadata(
            ocId: identifier,
            account: account.ncKitAccount,
            classFile: directory ? NKTypeClassFile.directory.rawValue : "",
            contentType: directory ? UTType.folder.identifier : "",
            creationDate: creationDate, // Use provided or fallback to default
            date: modificationDate, // Use provided or fallback to default
            directory: directory,
            e2eEncrypted: false, // Default as not set in original code
            etag: versionIdentifier,
            fileId: identifier.replacingOccurrences(of: trashedItemIdSuffix, with: ""),
            fileName: name,
            fileNameView: name,
            hasPreview: false, // Default as not set in original code
            iconName: "", // Placeholder as not set in original code
            mountType: "", // Placeholder as not set in original code
            name: name,
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            lock: locked,
            lockOwner: lockOwner,
            lockOwnerType: lockOwner.isEmpty ? 0 : 1,
            lockOwnerDisplayName: lockOwner == account.username ? account.username : "other user",
            lockTime: nil, // Default as not set in original code
            lockTimeOut: lockTimeOut,
            path: "", // Placeholder as not set in original code
            serverUrl: trimmedServerUrl,
            size: size,
            uploaded: true,
            trashbinFileName: trashbinOriginalLocation != nil ? fileName : "",
            trashbinOriginalLocation: trashbinOriginalLocation ?? "",
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
    }
}
