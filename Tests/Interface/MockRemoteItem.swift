//
//  MockRemoteItem.swift
//
//
//  Created by Claudio Cambra on 9/5/24.
//

import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import UniformTypeIdentifiers

public class MockRemoteItem: Equatable {
    public var parent: MockRemoteItem?
    public var children: [MockRemoteItem] = []

    public let identifier: String
    public let versionIdentifier: String
    public var name: String
    public var remotePath: String
    public let directory: Bool
    public let creationDate: Date
    public var modificationDate: Date
    public var data: Data?
    public var locked: Bool
    public var lockOwner: String
    public var lockTimeOut: Date?
    public var size: Int64 { Int64(data?.count ?? 0) }
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
        let file = NKFile()
        file.fileName = trashbinOriginalLocation?.split(separator: "/").last?.toString() ?? name
        file.size = size
        file.date = creationDate
        file.directory = directory
        file.etag = versionIdentifier
        file.ocId = identifier
        file.serverUrl = parent?.remotePath ?? remotePath
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
        let trashItem = NKTrash()
        trashItem.ocId = identifier
        trashItem.fileName = trashbinOriginalLocation?.split(separator: "/").last?.toString() ?? name
        trashItem.directory = directory
        trashItem.trashbinOriginalLocation = trashbinOriginalLocation ?? ""
        trashItem.trashbinFileName = name
        trashItem.size = size
        trashItem.filePath = (parent?.remotePath ?? "") + "/" + name
        return trashItem
    }

    public func toItemMetadata(account: Account) -> ItemMetadata {
        let originalFileName = trashbinOriginalLocation?.split(separator: "/").last?.toString()
        let fileName = originalFileName ?? name
        let serverUrlTrimCount = name.count

        let metadata = ItemMetadata()
        metadata.ocId = identifier
        metadata.etag = versionIdentifier
        metadata.directory = directory
        metadata.name = fileName
        metadata.fileName = fileName
        metadata.fileNameView = fileName
        metadata.trashbinOriginalLocation = trashbinOriginalLocation ?? ""
        metadata.serverUrl = remotePath
        metadata.serverUrl.removeSubrange(
            remotePath.index(
                remotePath.endIndex, offsetBy: -(serverUrlTrimCount + 1) // Remove trailing slash
            )..<remotePath.endIndex
        )
        metadata.date = modificationDate
        metadata.creationDate = creationDate
        metadata.size = data?.count as? Int64 ?? 0
        metadata.urlBase = account.serverUrl
        metadata.userId = account.id
        metadata.user = account.username
        metadata.account = account.ncKitAccount

        if (trashbinOriginalLocation != nil) {
            metadata.trashbinFileName = name
        }
        if directory {
            metadata.classFile = NKCommon.TypeClassFile.directory.rawValue
            metadata.contentType = UTType.folder.identifier
        }

        return metadata
    }
}
