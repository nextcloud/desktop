//
//  Item+Delete.swift
//
//
//  Created by Claudio Cambra on 15/4/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

public extension Item {

    func delete() async -> Error? {
        let serverFileNameUrl = metadata.serverUrl + "/" + metadata.fileName
        guard serverFileNameUrl != "" else {
            return NSFileProviderError(.noSuchItem)
        }
        let ocId = itemIdentifier.rawValue

        let error = await withCheckedContinuation { 
            (continuation: CheckedContinuation<Error?, Never>) -> Void in
            ncKit.deleteFileOrFolder(serverUrlFileName: serverFileNameUrl) { _, error in
                continuation.resume(returning: error.fileProviderError)
            }
        }

        guard error == nil else {
            Self.logger.error(
                """
                Could not delete item with ocId \(ocId, privacy: .public)...
                at \(serverFileNameUrl, privacy: .public)...
                received error: \(error?.localizedDescription ?? "", privacy: .public)
                """
            )
            return error
        }

        Self.logger.info(
            """
            Successfully deleted item with identifier: \(ocId, privacy: .public)...
            at: \(serverFileNameUrl, privacy: .public)
            """
        )

        let dbManager = FilesDatabaseManager.shared

        if self.metadata.directory {
            _ = dbManager.deleteDirectoryAndSubdirectoriesMetadata(ocId: ocId)
        } else {
            dbManager.deleteItemMetadata(ocId: ocId)
            if dbManager.localFileMetadataFromOcId(ocId) != nil {
                dbManager.deleteLocalFileMetadata(ocId: ocId)
            }
        }
        return nil
    }
}
