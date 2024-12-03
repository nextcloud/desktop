//
//  NKTrash+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

import Foundation
import NextcloudKit

extension NKTrash {
    func toItemMetadata(account: Account) -> ItemMetadata {
        let metadata = ItemMetadata()
        metadata.ocId = ocId
        metadata.account = account.ncKitAccount
        metadata.user = account.username
        metadata.userId = account.id
        metadata.urlBase = account.serverUrl
        metadata.contentType = contentType
        metadata.date = date
        metadata.directory = directory
        metadata.fileId = fileId
        metadata.fileName = fileName
        metadata.hasPreview = hasPreview
        metadata.iconName = iconName
        metadata.size = size
        metadata.classFile = classFile
        metadata.serverUrl = account.trashUrl
        metadata.trashbinFileName = trashbinFileName
        metadata.trashbinOriginalLocation = trashbinOriginalLocation
        metadata.trashbinDeletionTime = trashbinDeletionTime
        return metadata
    }
}
