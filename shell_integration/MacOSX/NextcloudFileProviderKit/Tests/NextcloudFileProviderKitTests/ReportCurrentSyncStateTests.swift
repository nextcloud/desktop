//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderXPC
import XCTest

/// Captures `reportSyncStatus(_:forDomainIdentifier:)` so the unconditional connect-time report can
/// be exercised without a real XPC connection.
private final class SyncStatusCapturingAppProxy: NSObject, AppProtocol {
    var reportedSyncStatuses: [String] = []

    func reportSyncStatus(_ status: String, forDomainIdentifier _: String) {
        reportedSyncStatuses.append(status)
    }

    // The following are unused by these tests but required for protocol conformance.
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

/// Regression coverage for https://github.com/nextcloud/desktop/issues/10053.
///
/// When the main app connects on launch, the extension must report its *current* state — even when
/// it is idle — so the app starts from an accurate status instead of falling back to the misleading
/// "Some files could not be synced!" message. The connect-time report previously went through the
/// edge-triggered `updatedSyncStateReporting(oldActions:)`, which deliberately bails when sync
/// activity has not changed; an idle extension therefore reported nothing at all. `reportCurrentSyncState()`
/// must instead always emit a report.
final class ReportCurrentSyncStateTests: NextcloudFileProviderKitTestCase {
    private func makeExtension() -> FileProviderExtension {
        let domain = NSFileProviderDomain(
            identifier: NSFileProviderDomainIdentifier("test-domain-10053"),
            displayName: "Test"
        )
        return FileProviderExtension(domain: domain)
    }

    /// The fresh-launch case from the bug report: no sync activity, so the resting state is "all good".
    func testReportsFinishedWhenIdle() {
        let ext = makeExtension()
        let proxy = SyncStatusCapturingAppProxy()
        ext.app = proxy

        ext.reportCurrentSyncState()

        XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_FINISHED"])
    }

    func testReportsStartedWhileSyncing() {
        let ext = makeExtension()
        let proxy = SyncStatusCapturingAppProxy()
        ext.app = proxy
        ext.syncActions.insert(UUID())

        ext.reportCurrentSyncState()

        XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_STARTED"])
    }

    func testReportsFailedWhenOnlyErrorsPending() {
        let ext = makeExtension()
        let proxy = SyncStatusCapturingAppProxy()
        ext.app = proxy
        ext.errorActions.insert(UUID())

        ext.reportCurrentSyncState()

        XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_FAILED"])
    }

    /// A missing app proxy (main app not running) must be a silent no-op, never a crash.
    func testWithoutProxyDoesNotCrash() {
        let ext = makeExtension()
        ext.reportCurrentSyncState()
    }
}
