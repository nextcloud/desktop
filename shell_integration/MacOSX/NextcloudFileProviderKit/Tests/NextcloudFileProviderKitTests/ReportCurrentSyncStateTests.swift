//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderXPC
import XCTest

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

    func testCompletedOrCancelledActionReportsFinished() {
        let errors: [Error?] = [nil, CocoaError(.userCancelled), NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError)]
        for error in errors {
            let ext = makeExtension()
            let proxy = SyncStatusCapturingAppProxy()
            ext.app = proxy
            let actionId = UUID()
            ext.insertSyncAction(actionId)

            ext.completeSyncAction(actionId, error: error)

            XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_STARTED", "SYNC_FINISHED"])
            XCTAssertTrue(ext.syncActions.isEmpty)
            XCTAssertTrue(ext.errorActions.isEmpty)
        }
    }

    func testFailedActionReportsFailure() {
        let ext = makeExtension()
        let proxy = SyncStatusCapturingAppProxy()
        ext.app = proxy
        let actionId = UUID()
        ext.insertSyncAction(actionId)

        ext.completeSyncAction(actionId, error: NSFileProviderError(.serverUnreachable))

        XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_STARTED", "SYNC_FAILED"])
        XCTAssertTrue(ext.syncActions.isEmpty)
    }

    func testCancelledActionPreservesOtherActionsAndFailures() {
        let ext = makeExtension()
        let proxy = SyncStatusCapturingAppProxy()
        ext.app = proxy
        let cancelledId = UUID()
        let runningId = UUID()
        let failedId = UUID()
        ext.insertSyncAction(cancelledId)
        ext.insertSyncAction(runningId)
        ext.insertSyncAction(failedId)
        ext.completeSyncAction(failedId, error: NSFileProviderError(.serverUnreachable))

        ext.completeSyncAction(cancelledId, error: CocoaError(.userCancelled))

        XCTAssertEqual(ext.syncActions, [runningId])
        XCTAssertEqual(ext.errorActions, [failedId])
        XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_STARTED"])
        ext.completeSyncAction(runningId, error: nil)
        XCTAssertEqual(proxy.reportedSyncStatuses, ["SYNC_STARTED", "SYNC_FAILED"])
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
