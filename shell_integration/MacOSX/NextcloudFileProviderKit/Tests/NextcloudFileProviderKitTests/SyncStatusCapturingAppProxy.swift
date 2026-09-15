//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudFileProviderXPC

///
/// Captures `reportSyncStatus(_:forDomainIdentifier:)` so that what the extension tells the app can be asserted without a real XPC connection.
///
/// Shared between the suites which care about synchronization status reporting, both those asserting what is reported and those asserting that nothing is.
///
final class SyncStatusCapturingAppProxy: NSObject, AppProtocol {
    ///
    /// Every status reported so far, in the order it was reported.
    ///
    var reportedSyncStatuses: [String] = []

    func reportSyncStatus(_ status: String, forDomainIdentifier _: String) {
        reportedSyncStatuses.append(status)
    }

    // The following are unused by the tests using this proxy but required for protocol conformance.
    func presentFileActions(_: String, path _: String, remoteItemPath _: String, withDomainIdentifier _: String) {}
    func openItemInBrowser(_: String, remoteItemPath _: String, forDomainIdentifier _: String) {}
    func copyInternalLink(forItem _: String, remoteItemPath _: String, forDomainIdentifier _: String) {}
    func reportItemExcluded(fromSync _: String, fileName _: String, reason _: String, forDomainIdentifier _: String) {}
    func reportInsufficientQuota(
        forItem _: String,
        fileName _: String,
        fileBytes _: NSNumber?,
        availableBytes _: NSNumber?,
        forDomainIdentifier _: String
    ) {}
    func reportInsufficientQuotaSummary(forDomainIdentifier _: String) {}
}
