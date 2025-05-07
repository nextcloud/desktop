//
//  Item+CreateLockFile.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider
import NextcloudCapabilitiesKit

extension Item {
    static func createLockFile(
        basedOn itemTemplate: NSFileProviderItem,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        parentItemRemotePath: String,
        progress: Progress,
        domain: NSFileProviderDomain? = nil,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager
    ) async -> (Item?, Error?) {
        progress.totalUnitCount = 1

        // Lock but don't upload, do not error
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )
        guard capabilitiesError == .success,
              let capabilities,
              capabilities.files?.locking != nil
        else {
            uploadLogger.info(
                """
                Received nil capabilities data.
                    Received error: \(capabilitiesError.errorDescription, privacy: .public)
                    Capabilities nil: \(capabilities == nil ? "YES" : "NO", privacy: .public)
                    (if capabilities are not nil the server may just not have files_lock enabled).
                    Will not proceed with locking for \(itemTemplate.filename, privacy: .public)
                """
            )
            return (nil, nil)
        }

        Self.logger.info(
            """
            Item to create:
                \(itemTemplate.filename, privacy: .public)
                is a lock file. Will handle by remotely locking the target file.
            """
        )
        guard let targetFileName = originalFileName(
            fromLockFileName: itemTemplate.filename
        ) else {
            Self.logger.error(
                """
                Could not get original filename from lock file filename
                    \(itemTemplate.filename, privacy: .public)
                    so will not lock target file.
                """
            )
            return (nil, nil)
        }
        let targetFileRemotePath = parentItemRemotePath + "/" + targetFileName
        let (_, _, error) = await remoteInterface.setLockStateForFile( // TODO: NOT WORKING
            remotePath: targetFileRemotePath,
            lock: true,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: itemTemplate.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )
        if error != .success {
            Self.logger.error(
                """
                Failed to lock target file \(targetFileName, privacy: .public)
                    for lock file: \(itemTemplate.filename, privacy: .public)
                    received error: \(error.errorDescription)
                """
            )
        } else {
            Self.logger.info("Locked file at: \(targetFileRemotePath, privacy: .public)")
        }

        let metadata = SendableItemMetadata(
            ocId: itemTemplate.itemIdentifier.rawValue,
            account: account.ncKitAccount,
            classFile: "lock", // Indicates this metadata is for a locked file
            contentType: itemTemplate.contentType?.preferredMIMEType ?? "",
            creationDate: itemTemplate.creationDate as? Date ?? Date(),
            date: Date(),
            directory: false,
            e2eEncrypted: false,
            etag: "",
            fileId: itemTemplate.itemIdentifier.rawValue,
            fileName: itemTemplate.filename,
            fileNameView: itemTemplate.filename,
            hasPreview: false,
            iconName: "lockIcon", // Custom icon for locked items
            mountType: "",
            ownerId: account.id,
            ownerDisplayName: "",
            path: parentItemRemotePath + "/" + targetFileName,
            serverUrl: parentItemRemotePath,
            size: 0,
            status: Status.normal.rawValue,
            downloaded: true,
            uploaded: false,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id
        )
        dbManager.addItemMetadata(metadata)

        progress.completedUnitCount = 1
        var returnError = error.fileProviderError // No need to handle problem cases, no upload here
        if #available(macOS 13.0, *), error == .success {
            returnError = NSFileProviderError(.excludedFromSync)
        }

        return (
            Item(
                metadata: metadata,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager
            ),
            returnError
        )
    }
}
