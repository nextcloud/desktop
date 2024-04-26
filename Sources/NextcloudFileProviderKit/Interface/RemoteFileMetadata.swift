//
//  RemoteFileData.swift
//
//
//  Created by Claudio Cambra on 26/4/24.
//

import Foundation

public protocol RemoteFileMetadata {
    var account: String { get }
    var identifier: String { get }
    var versionIdentifier: String { get }
    var parentContainerUrl: String { get }
    var isFolder: Bool { get }
    var creationDate: NSDate? { get }
    var lastUsedDate: Date? { get }
    var contentModificationDate: Date? { get }
    var size: Int64 { get } // Bytes
    var name: String { get }
}
