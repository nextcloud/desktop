//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import RealmSwift

extension RealmItemMetadata {
    static func hasLocation(
        _ item: Query<RealmItemMetadata>,
        account: String,
        serverUrl: String,
        fileName: String
    ) -> Query<Bool> {
        let canonicalServerUrl = serverUrl.canonicalForm
        let canonicalFileName = fileName.canonicalForm

        return item.account == account
            && (item.normalizedServerUrl == canonicalServerUrl
                || (item.normalizedServerUrl == "" && item.serverUrl == serverUrl))
            && (item.normalizedFileName == canonicalFileName
                || (item.normalizedFileName == "" && item.fileName == fileName))
    }

    static func hasServerUrl(
        _ item: Query<RealmItemMetadata>,
        equalTo serverUrl: String,
        includingDescendants: Bool
    ) -> Query<Bool> {
        let canonicalServerUrl = serverUrl.canonicalForm
        let canonicalPrefix = canonicalServerUrl + "/"

        if includingDescendants {
            return item.normalizedServerUrl == canonicalServerUrl
                || item.normalizedServerUrl.starts(with: canonicalPrefix)
                || (item.normalizedServerUrl == ""
                    && (item.serverUrl == serverUrl || item.serverUrl.starts(with: serverUrl + "/")))
        }

        return item.normalizedServerUrl == canonicalServerUrl
            || (item.normalizedServerUrl == "" && item.serverUrl == serverUrl)
    }
}
