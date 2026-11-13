//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

/// Which of the account's WebDAV roots contains an item.
enum DAVRoot: String, CaseIterable, Sendable {
    case files
    case trash

    func rootURL(accountBaseURL: String, backendUserID: String) -> String {
        switch self {
            case .files:
                return accountBaseURL + Account.webDavFilesUrlSuffix + backendUserID
            case .trash:
                return accountBaseURL + Account.webDavTrashUrlSuffix + backendUserID + "/trash"
        }
    }
}
