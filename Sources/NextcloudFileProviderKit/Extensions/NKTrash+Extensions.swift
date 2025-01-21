//
//  NKTrash+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-02.
//

import Foundation
import NextcloudKit

extension NKTrash {
    func toItemMetadata(account: Account) -> SendableItemMetadata {
        SendableItemMetadata(
            ocId: ocId,
            account: account.ncKitAccount,
            checksums: "", // Placeholder as not set in original code
            chunkUploadId: "", // Placeholder as not set in original code
            classFile: classFile,
            commentsUnread: false, // Default as not set in original code
            contentType: contentType,
            creationDate: Date(), // Default as not set in original code
            dataFingerprint: "", // Placeholder as not set in original code
            date: date,
            directory: directory,
            downloadURL: "", // Placeholder as not set in original code
            e2eEncrypted: false, // Default as not set in original code
            etag: "", // Placeholder as not set in original code
            favorite: false, // Default as not set in original code
            fileId: fileId,
            fileName: fileName,
            fileNameView: trashbinFileName,
            hasPreview: hasPreview,
            iconName: iconName,
            iconUrl: "", // Placeholder as not set in original code
            livePhoto: false, // Default as not set in original code
            mountType: "", // Placeholder as not set in original code
            name: "", // Placeholder as not set in original code
            note: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            lock: false, // Default as not set in original code
            lockOwner: "", // Placeholder as not set in original code
            lockOwnerEditor: "", // Placeholder as not set in original code
            lockOwnerType: 0, // Default as not set in original code
            lockOwnerDisplayName: "", // Placeholder as not set in original code
            lockTime: nil, // Default as not set in original code
            lockTimeOut: nil, // Default as not set in original code
            path: "", // Placeholder as not set in original code
            permissions: "", // Placeholder as not set in original code
            quotaUsedBytes: 0, // Default as not set in original code
            quotaAvailableBytes: 0, // Default as not set in original code
            resourceType: "", // Placeholder as not set in original code
            richWorkspace: nil, // Default as not set in original code
            serverUrl: account.trashUrl,
            session: "", // Placeholder as not set in original code
            sessionError: "", // Placeholder as not set in original code
            sessionTaskIdentifier: 0, // Default as not set in original code
            sharePermissionsCollaborationServices: 0, // Default as not set in original code
            sharePermissionsCloudMesh: [], // Default as not set in original code
            size: size,
            status: 0, // Default as not set in original code
            downloaded: false, // Default as not set in original code
            uploaded: false, // Default as not set in original code
            trashbinFileName: trashbinFileName,
            trashbinOriginalLocation: trashbinOriginalLocation,
            trashbinDeletionTime: trashbinDeletionTime,
            uploadDate: Date(), // Default as not set in original code
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
    }
}
