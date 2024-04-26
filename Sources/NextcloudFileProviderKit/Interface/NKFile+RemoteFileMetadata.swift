//
//  NKFile+RemoteFileMetadata.swift
//
//
//  Created by Claudio Cambra on 26/4/24.
//

import Foundation
import NextcloudKit

extension NKFile: RemoteFileMetadata {
    public var lastUsedDate: Date? { self.date as Date? }
    public var contentModificationDate: Date? { self.date as Date? }
    public var identifier: String { self.ocId }
    public var versionIdentifier: String { self.etag }
    public var parentContainerUrl: String { self.serverUrl }
    public var isFolder: Bool { self.directory }
}
