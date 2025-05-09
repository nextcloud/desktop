//
//  Item+CreateIgnored.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider
import NextcloudKit

extension Item {
    static func createIgnored(
        basedOn itemTemplate: NSFileProviderItem,
        parentItemRemotePath: String,
        contents url: URL?,
        account: Account,
        remoteInterface: RemoteInterface,
        progress: Progress,
        dbManager: FilesDatabaseManager
    ) -> (Item?, Error?) {
        let filename = itemTemplate.filename
        Self.logger.info(
            """
            File \(filename, privacy: .public) is in the ignore list.
                \(parentItemRemotePath + "/" + filename, privacy: .public)
                Will not propagate creation to server.
            """
        )
        let metadata = SendableItemMetadata(
            ocId: itemTemplate.itemIdentifier.rawValue,
            account: account.ncKitAccount,
            classFile: NKCommon.TypeClassFile.unknow.rawValue,
            contentType: itemTemplate.contentType?.preferredMIMEType ?? "",
            creationDate: itemTemplate.creationDate as? Date ?? Date(),
            date: itemTemplate.contentModificationDate as? Date ?? Date(),
            directory: itemTemplate.contentType?.conforms(to: .directory) ?? false,
            e2eEncrypted: false,
            etag: "",
            fileId: itemTemplate.itemIdentifier.rawValue,
            fileName: itemTemplate.filename,
            fileNameView: itemTemplate.filename,
            hasPreview: false,
            iconName: "",
            mountType: "",
            ownerId: account.id,
            ownerDisplayName: "",
            path: "",
            serverUrl: parentItemRemotePath,
            size: itemTemplate.documentSize??.int64Value ?? 0,
            status: Status.normal.rawValue,
            downloaded: true,
            uploaded: false,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
        dbManager.addItemMetadata(metadata)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: itemTemplate.parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )

        if #available(macOS 13.0, *) {
            return (item, NSFileProviderError(.excludedFromSync))
        } else {
            return (item, nil)
        }
    }
}
