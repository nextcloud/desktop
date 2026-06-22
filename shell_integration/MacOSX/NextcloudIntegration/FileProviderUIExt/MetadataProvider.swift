//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import OSLog

func fetchItemMetadata(itemRelativePath: String, account: Account, kit: NextcloudKit) async -> NKFile? {
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

    guard let nksession = kit.nkCommonInstance.nksessions.session(forAccount: account.ncKitAccount) else {
        return nil
    }

    let urlBase = slashlessPath(nksession.urlBase)
    let davSuffix = slashlessPath(nksession.dav)
    let userId = nksession.userId
    let itemRelPath = slashlessPath(itemRelativePath)
    let itemFullServerPath = "\(urlBase)/\(davSuffix)/files/\(userId)/\(itemRelPath)"

    return await withCheckedContinuation { continuation in
        kit.readFileOrFolder(serverUrlFileName: itemFullServerPath, depth: "0", account: account.ncKitAccount) { account, files, data, error in
            guard error == .success else {
                continuation.resume(returning: nil)
                return
            }

            continuation.resume(returning: files?.first)
        }
    }
}
