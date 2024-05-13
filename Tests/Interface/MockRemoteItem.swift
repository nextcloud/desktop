//
//  MockRemoteItem.swift
//
//
//  Created by Claudio Cambra on 9/5/24.
//

import Foundation

public class MockRemoteItem: Equatable {
    public var parent: MockRemoteItem?
    public var children: [MockRemoteItem] = []

    public let identifier: String
    public let versionIdentifier: String
    public let name: String
    public let directory: Bool
    public let size: Int64
    public let creationDate: Date
    public let modificationDate: Date

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
        directory: Bool = false,
        size: Int64 = 0,
        creationDate: Date = Date(),
        modificationDate: Date = Date()
    ) {
        self.identifier = identifier
        self.versionIdentifier = versionIdentifier
        self.name = name
        self.directory = directory
        self.size = size
        self.creationDate = creationDate
        self.modificationDate = modificationDate
    }
}
