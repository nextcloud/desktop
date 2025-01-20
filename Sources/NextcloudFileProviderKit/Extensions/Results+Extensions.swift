//
//  Results+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-18.
//

import Realm
import RealmSwift

extension Results where Element: RemoteFileChunk {
    func toUnmanagedResults() -> [RemoteFileChunk] {
        return map { RemoteFileChunk(value: $0) }
    }
}

extension Results where Element: ItemMetadata {
    func toUnmanagedResults() -> [SendableItemMetadata] {
        return map { SendableItemMetadata(value: $0) }
    }
}



