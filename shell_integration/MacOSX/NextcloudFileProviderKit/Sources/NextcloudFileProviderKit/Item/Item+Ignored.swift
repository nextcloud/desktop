//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Item {
    static func createIgnored(
        basedOn itemTemplate: NSFileProviderItem,
        parentItemRemotePath: String,
        contents _: URL?,
        account: Account,
        remoteInterface: RemoteInterface,
        progress _: Progress,
        dbManager: FilesDatabaseManager,
        log: any FileProviderLogging
    ) async -> (Item?, Error?) {
        let filename = itemTemplate.filename
        let logger = FileProviderLogger(category: "Item", log: log)

        logger.info(
            """
            File \(filename) is in the ignore list.
                \(parentItemRemotePath + "/" + filename)
                Will not propagate creation to server.
            """
        )

        let metadata = SendableItemMetadata(
            ocId: itemTemplate.itemIdentifier.rawValue,
            account: account.ncKitAccount,
            classFile: NKTypeClassFile.unknow.rawValue,
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

        let item = await Item(
            metadata: metadata,
            parentItemIdentifier: itemTemplate.parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: false,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: account),
            log: log
        )

        return (item, NSFileProviderError(.excludedFromSync))
    }
}
