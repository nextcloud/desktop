//
//  RemoteFileChunk.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2025-01-08.
//

import Foundation
import NextcloudKit
import RealmSwift

public class RemoteFileChunk: Object {
    @Persisted public var fileName: String
    @Persisted public var size: Int64
    @Persisted public var uploadUuid: String
}

