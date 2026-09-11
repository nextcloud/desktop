//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import RealmSwift

extension RealmItemMetadata {
    ///
    /// Match the row occupying a logical address, comparing on the indexed normalized keys alone so
    /// Realm can answer it without scanning the table.
    ///
    static func hasLocation(
        _ item: Query<RealmItemMetadata>,
        serverUrl: String,
        fileName: String
    ) -> Query<Bool> {
        // File name first, because both conjuncts cost the same and Realm breaks the tie by source
        // order, so the query drives off the near-unique name rather than the whole sibling set.
        item.normalizedFileName == fileName.precomposedStringWithCanonicalMapping
            && item.normalizedServerUrl == serverUrl.precomposedStringWithCanonicalMapping
    }

    /// Match rows in a directory, optionally including everything beneath it, comparing only the
    /// normalized key as ``hasLocation`` does.
    static func hasServerUrl(
        _ item: Query<RealmItemMetadata>,
        equalTo serverUrl: String,
        includingDescendants: Bool
    ) -> Query<Bool> {
        let canonicalServerUrl = serverUrl.precomposedStringWithCanonicalMapping

        if includingDescendants {
            return item.normalizedServerUrl == canonicalServerUrl
                || item.normalizedServerUrl.starts(with: canonicalServerUrl + "/")
        }

        return item.normalizedServerUrl == canonicalServerUrl
    }
}
