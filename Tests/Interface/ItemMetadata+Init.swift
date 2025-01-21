//
//  ItemMetadata+Init.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2025-01-21.
//

import Foundation
import NextcloudFileProviderKit

public extension SendableItemMetadata {
    init() {
        self.init(
            ocId: "",
            account: "",
            checksums: "",
            chunkUploadId: "",
            classFile: "",
            commentsUnread: false,
            contentType: "",
            creationDate: Date(),
            dataFingerprint: "",
            date: Date(),
            directory: false,
            downloadURL: "",
            e2eEncrypted: false,
            etag: "",
            favorite: false,
            fileId: "",
            fileName: "",
            fileNameView: "",
            hasPreview: false,
            iconName: "",
            iconUrl: "",
            livePhoto: false,
            mountType: "",
            name: "",
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
            serverUrl: "",
            session: "",
            sessionError: "",
            sessionTaskIdentifier: 0,
            sharePermissionsCollaborationServices: 0,
            sharePermissionsCloudMesh: [],
            size: 0,
            status: 0,
            downloaded: false,
            uploaded: false,
            trashbinFileName: "",
            trashbinOriginalLocation: "",
            trashbinDeletionTime: Date(),
            uploadDate: Date(),
            urlBase: "",
            user: "",
            userId: ""
        )
    }
}
