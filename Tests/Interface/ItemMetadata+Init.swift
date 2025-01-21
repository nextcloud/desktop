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
            date: Date(),
            directory: false,
            e2eEncrypted: false,
            etag: "",
            favorite: false,
            fileId: "",
            fileName: fileName,
            fileNameView: fileName,
            hasPreview: false,
            iconName: "",
            livePhoto: false,
            mountType: "",
            name: fileName,
            note: "",
            ownerId: "",
            ownerDisplayName: "",
            lock: false,
            lockOwner: "",
            lockOwnerEditor: "",
            lockOwnerType: 0,
            lockOwnerDisplayName: "",
            lockTime: nil,
            lockTimeOut: nil,
            path: "",
            permissions: "",
            quotaUsedBytes: 0,
            quotaAvailableBytes: 0,
            resourceType: "",
            richWorkspace: nil,
            serverUrl: account.davFilesUrl,
            session: "",
            sessionError: "",
            sessionTaskIdentifier: 0,
            sharePermissionsCollaborationServices: 0,
            sharePermissionsCloudMesh: [],
            size: 0,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
    }
}
