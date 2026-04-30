//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudFileProviderXPC
import XCTest

/// Captures `AppProtocol` calls in-memory so the reporter can be exercised without a real XPC
/// connection.
private final class CapturingAppProxy: NSObject, AppProtocol {
    struct Capture: Equatable {
        let relativePath: String
        let fileName: String
        let reason: String
        let domainIdentifier: String
    }

    var presentedFileActions: [String] = []
    var reportedSyncStatuses: [String] = []
    var captured: [Capture] = []

    func presentFileActions(
        _ fileId: String,
        path: String,
        remoteItemPath: String,
        withDomainIdentifier domainIdentifier: String
    ) {
        presentedFileActions.append(fileId)
    }

    func reportSyncStatus(_ status: String, forDomainIdentifier domainIdentifier: String) {
        reportedSyncStatuses.append(status)
    }

    func reportItemExcluded(
        fromSync relativePath: String,
        fileName: String,
        reason: String,
        forDomainIdentifier domainIdentifier: String
    ) {
        captured.append(
            Capture(
                relativePath: relativePath,
                fileName: fileName,
                reason: reason,
                domainIdentifier: domainIdentifier
            )
        )
    }
}

final class BundleExclusionReporterTests: XCTestCase {
    func testReportSendsExpectedArgumentsToProxy() {
        let proxy = CapturingAppProxy()

        BundleExclusionReporter.report(
            relativePath: "/Inbox/PluginKit Monitor.app",
            fileName: "PluginKit Monitor.app",
            domainIdentifier: NSFileProviderDomainIdentifier("test-domain"),
            appProxy: proxy,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(proxy.captured.count, 1)
        let capture = try? XCTUnwrap(proxy.captured.first)
        XCTAssertEqual(capture?.relativePath, "/Inbox/PluginKit Monitor.app")
        XCTAssertEqual(capture?.fileName, "PluginKit Monitor.app")
        XCTAssertEqual(capture?.domainIdentifier, "test-domain")
        XCTAssertFalse(capture?.reason.isEmpty ?? true, "Reason text must not be empty.")
    }

    func testReportWithoutProxyDropsSilently() {
        // No assertion crash, no exception, just an info-level log line.
        BundleExclusionReporter.report(
            relativePath: "/Inbox/foo.app",
            fileName: "foo.app",
            domainIdentifier: NSFileProviderDomainIdentifier("test-domain"),
            appProxy: nil,
            log: FileProviderLogMock()
        )
    }

    func testReasonTextIsNonEmpty() {
        XCTAssertFalse(BundleExclusionReporter.reasonText().isEmpty)
    }
}
