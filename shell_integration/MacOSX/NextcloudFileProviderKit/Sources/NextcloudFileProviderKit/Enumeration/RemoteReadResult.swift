//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudKit

///
/// The outcome of reading a remote path via ``Enumerator/readServerUrl(_:pageSettings:account:remoteInterface:dbManager:domain:enumeratedItemIdentifier:depth:log:)``.
///
/// Replaces the positional six-element tuple that callers used to destructure as
/// `(_, _, _, _, _, error)`. Field access (`result.metadatas`, `result.error`, …) documents at
/// each call site which parts of the read it actually consumes.
///
struct RemoteReadResult {
    ///
    /// The items read from the server. For depth-1 reads the target directory is the first element.
    /// `nil` when the read failed (see ``error``).
    ///
    var metadatas: [SendableItemMetadata]?

    ///
    /// What changed relative to the local database.
    ///
    /// `nil` for paginated reads — which only report the items discovered on the page and do not
    /// diff against the database — and `nil` when the database write that derives the change set
    /// failed. Change enumeration treats a `nil` change set on a non-paginated read as an error.
    ///
    var changes: ChangeSet?

    ///
    /// The pagination continuation, present only when the server returned a page token.
    ///
    var nextPage: EnumeratorPageResponse?

    ///
    /// The error that ended the read, or `nil` on success.
    ///
    var error: NKError?

    init(
        metadatas: [SendableItemMetadata]? = nil,
        changes: ChangeSet? = nil,
        nextPage: EnumeratorPageResponse? = nil,
        error: NKError? = nil
    ) {
        self.metadatas = metadatas
        self.changes = changes
        self.nextPage = nextPage
        self.error = error
    }
}
