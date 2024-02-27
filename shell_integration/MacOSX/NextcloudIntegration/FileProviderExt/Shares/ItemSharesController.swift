//
//  ItemSharesController.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 27/2/24.
//

import FileProvider
import Foundation
import NextcloudKit
import OSLog

class ItemSharesController {
    let itemIdentifier: NSFileProviderItemIdentifier
    let parentExtension: FileProviderExtension

    init(itemIdentifier: NSFileProviderItemIdentifier, parentExtension: FileProviderExtension) {
        self.itemIdentifier = itemIdentifier
        self.parentExtension = parentExtension
    }

    func fetch() async -> [NKShare]? {
        let rawIdentifier = itemIdentifier.rawValue
        Logger.shares.info("Fetching shares for item \(rawIdentifier, privacy: .public)")

        guard let baseUrl = parentExtension.ncAccount?.davFilesUrl else {
            Logger.shares.error("Could not fetch shares as ncAccount on parent extension is nil")
            return nil
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        guard let item = dbManager.itemMetadataFromFileProviderItemIdentifier(itemIdentifier) else {
            Logger.shares.error("No item \(rawIdentifier, privacy: .public) in db, no shares.")
            return nil
        }

        let completePath = item.serverUrl + "/" + item.fileName
        let relativePath = completePath.replacingOccurrences(of: baseUrl, with: "")
        let parameter = NKShareParameter(path: relativePath)

        return await withCheckedContinuation { continuation in
            let kit = parentExtension.ncKit
            kit.readShares(parameters: parameter) { account, shares, data, error in
                defer { continuation.resume(returning: shares) }
                guard error == .success else {
                    Logger.shares.error("Error fetching shares for \(rawIdentifier): \(error)")
                    return
                }
            }
        }
    }
}
