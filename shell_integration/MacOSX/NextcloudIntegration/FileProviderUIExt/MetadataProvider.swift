//
//  MetadataProvider.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 30/7/24.
//

import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import OSLog

func fetchItemMetadata(
    itemRelativePath: String, account: Account, kit: NextcloudKit
) async -> NKFile? {
    func slashlessPath(_ string: String) -> String {
        var strCopy = string
        if strCopy.hasPrefix("/") {
            strCopy.removeFirst()
        }
        if strCopy.hasSuffix("/") {
            strCopy.removeLast()
        }
        return strCopy
    }

    guard let nksession = kit.getSession(account: account.ncKitAccount) else {
        Logger.metadataProvider.error("Could not get nksession for \(account.ncKitAccount)")
        return nil
    }

    let urlBase = slashlessPath(nksession.urlBase)
    let davSuffix = slashlessPath(nksession.dav)
    let userId = nksession.userId
    let itemRelPath = slashlessPath(itemRelativePath)

    let itemFullServerPath = "\(urlBase)/\(davSuffix)/files/\(userId)/\(itemRelPath)"
    return await withCheckedContinuation { continuation in
        kit.readFileOrFolder(
            serverUrlFileName: itemFullServerPath, depth: "0", account: account.ncKitAccount
        ) {
            account, files, data, error in
            guard error == .success else {
                Logger.metadataProvider.error(
                    "Error getting item metadata: \(error.errorDescription)"
                )
                continuation.resume(returning: nil)
                return
            }
            Logger.metadataProvider.info("Successfully retrieved item metadata")
            continuation.resume(returning: files?.first)
        }
    }
}
