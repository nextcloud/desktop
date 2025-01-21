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
            assetLocalIdentifier: "",
            checksums: "",
            chunkUploadId: "",
            classFile: "",
            commentsUnread: false,
            contentType: "",
            creationDate: Date(),
            dataFingerprint: "",
            date: Date(),
            directory: false,
            deleteAssetLocalIdentifier: false,
            downloadURL: "",
            e2eEncrypted: false,
            edited: false,
            etag: "",
            etagResource: "",
            favorite: false,
            fileId: "",
            fileName: "",
            fileNameView: "",
            hasPreview: false,
            iconName: "",
            iconUrl: "",
            isExtractFile: false,
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
            sessionSelector: "",
            sessionTaskIdentifier: 0,
            sharePermissionsCollaborationServices: 0,
            sharePermissionsCloudMesh: [],
            size: 0,
            status: 0,
            downloaded: false,
            uploaded: false,
            subline: nil,
            trashbinFileName: "",
            trashbinOriginalLocation: "",
            trashbinDeletionTime: Date(),
            uploadDate: Date(),
            url: "",
            urlBase: "",
            user: "",
            userId: ""
        )
    }
}
