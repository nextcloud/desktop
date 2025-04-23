//
//  Item+DeleteLockFile.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 17/4/25.
//

import FileProvider
import NextcloudCapabilitiesKit

extension Item {
    func deleteLockFile(
        domain: NSFileProviderDomain? = nil, dbManager: FilesDatabaseManager
    ) async -> Error? {
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
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
                    Will not proceed with unlocking for \(self.filename, privacy: .public)
                """
            )
            return nil
        }

        dbManager.deleteItemMetadata(ocId: metadata.ocId)

        guard let originalFileName = originalFileName(
            fromLockFileName: metadata.fileName
        ) else {
            Self.logger.error(
                """
                Could not get original filename from lock file filename
                    \(self.metadata.fileName, privacy: .public)
                    so will not unlock target file.
                """
            )
            return nil
        }
        let originalFileServerFileNameUrl = metadata.serverUrl + "/" + originalFileName
        let (_, _, error) = await remoteInterface.setLockStateForFile(
            remotePath: originalFileServerFileNameUrl,
            lock: false,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: self.itemIdentifier,
                        completionHandler: { _ in }
                    )
                }
            }
        )
        guard error == .success else {
            Self.logger.error(
                """
                Could not unlock item for \(self.filename, privacy: .public)...
                    at \(originalFileServerFileNameUrl, privacy: .public)...
                    received error: \(error.errorCode, privacy: .public)
                    \(error.errorDescription, privacy: .public)
                """
            )
            return error.fileProviderError
        }
        Self.logger.info(
            """
            Successfully unlocked item for: \(self.filename, privacy: .public)...
                at: \(originalFileServerFileNameUrl, privacy: .public)
            """
        )
        return nil
    }
}
