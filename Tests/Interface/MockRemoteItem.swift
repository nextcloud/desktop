//
//  MockRemoteItem.swift
//
//
//  Created by Claudio Cambra on 9/5/24.
//

import Foundation
import NextcloudKit

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
    public var size: Int64 { Int64(data?.count ?? 0) }
    public var nkfile: NKFile {
        let file = NKFile()
        file.fileName = name
        file.size = size
        file.date = creationDate as NSDate
        file.directory = directory
        file.etag = versionIdentifier
        file.ocId = identifier
        file.serverUrl = parent?.remotePath ?? remotePath
        return file
    }

    public static func == (lhs: MockRemoteItem, rhs: MockRemoteItem) -> Bool {
        lhs.parent == rhs.parent &&
        lhs.children == rhs.children &&
        lhs.identifier == rhs.identifier &&
        lhs.versionIdentifier == rhs.versionIdentifier &&
        lhs.name == rhs.name &&
        lhs.directory == rhs.directory &&
        lhs.size == rhs.size &&
        lhs.creationDate == rhs.creationDate &&
        lhs.modificationDate == rhs.modificationDate
    }

    public init(
        identifier: String,
        versionIdentifier: String = "0",
        name: String,
        remotePath: String,
        directory: Bool = false,
        creationDate: Date = Date(),
        modificationDate: Date = Date(),
        data: Data? = nil
    ) {
        self.identifier = identifier
        self.versionIdentifier = versionIdentifier
        self.name = name
        self.remotePath = remotePath
        self.directory = directory
        self.creationDate = creationDate
        self.modificationDate = modificationDate
        self.data = data
    }
}
