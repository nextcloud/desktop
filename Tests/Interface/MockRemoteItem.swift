//
//  MockRemoteItem.swift
//
//
//  Created by Claudio Cambra on 9/5/24.
//

import Foundation

public class MockRemoteItem {
    public var parent: MockRemoteItem?
    public var children: [MockRemoteItem] = []

    public let identifier: String
    public let versionIdentifier: String
    public let name: String
    public let directory: Bool
    public let size: Int64
    public let creationDate: Date
    public let modificationDate: Date

    public init(
        identifier: String,
        versionIdentifier: String,
        name: String,
        directory: Bool,
        size: Int64,
        creationDate: Date,
        modificationDate: Date
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
