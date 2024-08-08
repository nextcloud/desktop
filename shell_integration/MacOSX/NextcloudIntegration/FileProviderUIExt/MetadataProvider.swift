//
//  MetadataProvider.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 30/7/24.
//

import Foundation
import NextcloudKit
import OSLog

func fetchItemMetadata(itemRelativePath: String, kit: NextcloudKit) async -> NKFile? {
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

    let nkCommon = kit.nkCommonInstance
    let urlBase = slashlessPath(nkCommon.urlBase)
    let davSuffix = slashlessPath(nkCommon.dav)
    let userId = nkCommon.userId
    let itemRelPath = slashlessPath(itemRelativePath)

    let itemFullServerPath = "\(urlBase)/\(davSuffix)/files/\(userId)/\(itemRelPath)"
    return await withCheckedContinuation { continuation in
        kit.readFileOrFolder(serverUrlFileName: itemFullServerPath, depth: "0") {
            account, files, data, error in
            guard error == .success else {
                Logger.metadataProvider.error(
                    "Error getting item metadata: \(error.errorDescription)"
                )
                continuation.resume(returning: nil)
                return
            }
            Logger.metadataProvider.info("Successfully retrieved item metadata")
            continuation.resume(returning: files.first)
        }
    }
}
