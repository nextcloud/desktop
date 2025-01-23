//
//  ItemMetadata+Init.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2025-01-21.
//

import Foundation
import NextcloudFileProviderKit

public extension SendableItemMetadata {
    init(ocId: String, fileName: String, account: Account) {
        self.init(
            ocId: ocId,
            account: account.ncKitAccount,
            classFile: "",
            contentType: "",
            creationDate: Date(),
            directory: false,
            e2eEncrypted: false,
            etag: "",
            fileId: "",
            fileName: fileName,
            fileNameView: fileName,
            hasPreview: false,
            iconName: "",
            mountType: "",
            name: fileName,
            ownerId: "",
            ownerDisplayName: "",
            path: "",
            serverUrl: account.davFilesUrl,
            size: 0,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
    }
}
