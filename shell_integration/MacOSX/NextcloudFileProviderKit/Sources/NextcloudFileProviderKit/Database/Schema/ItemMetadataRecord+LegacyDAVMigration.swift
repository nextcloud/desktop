//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation

extension ItemMetadataRecord {
    /// Older synthetic root and trash rows omitted these fields. Read the base from the account string and the user ID from the DAV URL.
    static func recoverServerBaseAndUserIDForLegacyContainer(
        ocId: String,
        account: String,
        parentURL: String
    ) -> (serverBaseURL: String, backendUserID: String)? {
        let isRoot = ocId == NSFileProviderItemIdentifier.rootContainer.rawValue
        guard isRoot || ocId == NSFileProviderItemIdentifier.trashContainer.rawValue else { return nil }
        let schemeStart = account.range(of: "://")?.lowerBound ?? account.endIndex
        guard let accountSeparator = account[..<schemeStart].lastIndex(of: " ") else { return nil }

        let serverBaseURL = String(account[account.index(after: accountSeparator)...])
        let suffix = isRoot ? Account.webDavFilesUrlSuffix : Account.webDavTrashUrlSuffix
        guard let davPrefixRange = parentURL.range(of: suffix, options: .backwards) else { return nil }

        var backendUserID = parentURL[davPrefixRange.upperBound...]
        if !isRoot {
            guard backendUserID.hasSuffix("/trash") else { return nil }
            backendUserID = backendUserID.dropLast("/trash".count)
        }
        guard !backendUserID.isEmpty, !backendUserID.contains("/") else { return nil }

        let location = Account.davRelativePath(forURL: parentURL, accountBaseURL: serverBaseURL, backendUserID: String(backendUserID))
        guard location == (isRoot ? "files:" : "trash:") else { return nil }
        return (serverBaseURL, String(backendUserID))
    }
}
