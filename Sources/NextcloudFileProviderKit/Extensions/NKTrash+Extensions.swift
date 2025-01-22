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
            classFile: classFile,
            contentType: contentType,
            creationDate: Date(), // Default as not set in original code
            date: date,
            directory: directory,
            e2eEncrypted: false, // Default as not set in original code
            etag: "", // Placeholder as not set in original code
            favorite: false, // Default as not set in original code
            fileId: fileId,
            fileName: fileName,
            fileNameView: trashbinFileName,
            hasPreview: hasPreview,
            iconName: iconName,
            livePhoto: false, // Default as not set in original code
            mountType: "", // Placeholder as not set in original code
            name: "", // Placeholder as not set in original code
            note: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
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
            trashbinFileName: trashbinFileName,
            trashbinOriginalLocation: trashbinOriginalLocation,
            trashbinDeletionTime: trashbinDeletionTime,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
    }
}
