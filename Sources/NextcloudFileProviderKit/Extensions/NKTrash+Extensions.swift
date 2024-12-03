//
//  NKTrash+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

import Foundation
import NextcloudKit

extension NKTrash {
    func toItemMetadata(account: String) -> ItemMetadata {
        let metadata = ItemMetadata()
        metadata.ocId = ocId
        metadata.account = account
        metadata.contentType = contentType
        metadata.date = date as Date
        metadata.directory = directory
        metadata.fileId = fileId
        metadata.fileName = fileName
        metadata.hasPreview = hasPreview
        metadata.iconName = iconName
        metadata.size = size
        metadata.classFile = classFile
        metadata.trashbinFileName = trashbinFileName
        metadata.trashbinOriginalLocation = trashbinOriginalLocation
        metadata.trashbinDeletionTime = trashbinDeletionTime as Date
        return metadata
    }
}
