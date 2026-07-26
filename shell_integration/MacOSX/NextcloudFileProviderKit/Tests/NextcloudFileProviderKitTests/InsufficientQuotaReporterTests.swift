//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudFileProviderXPC
import XCTest

/// Captures `AppProtocol` calls in-memory so the reporter can be exercised without a real XPC
/// connection. Mirrors the helper used by `BundleExclusionReporterTests`, but with capture
/// arrays for the new quota-specific methods.
private final class CapturingAppProxy: NSObject, AppProtocol {
    struct ItemCapture: Equatable {
        let relativePath: String
        let fileName: String
        let fileBytes: Int64?
        let availableBytes: Int64?
        let domainIdentifier: String
    }

    var capturedItems: [ItemCapture] = []
    var capturedSummaryDomains: [String] = []

    func presentFileActions(
        _: String, path _: String, remoteItemPath _: String, withDomainIdentifier _: String
    ) {}

    func openItemInBrowser(_: String, remoteItemPath _: String, forDomainIdentifier _: String) {}

    func copyInternalLink(forItem _: String, remoteItemPath _: String, forDomainIdentifier _: String) {}

    func reportSyncStatus(_: String, forDomainIdentifier _: String) {}

    func reportItemExcluded(
        fromSync _: String,
        fileName _: String,
        reason _: String,
        forDomainIdentifier _: String
    ) {}

    func reportInsufficientQuota(
        forItem relativePath: String,
        fileName: String,
        fileBytes: NSNumber?,
        availableBytes: NSNumber?,
        forDomainIdentifier domainIdentifier: String
    ) {
        capturedItems.append(
            ItemCapture(
                relativePath: relativePath,
                fileName: fileName,
                fileBytes: fileBytes?.int64Value,
                availableBytes: availableBytes?.int64Value,
                domainIdentifier: domainIdentifier
            )
        )
    }

    func reportInsufficientQuotaSummary(forDomainIdentifier domainIdentifier: String) {
        capturedSummaryDomains.append(domainIdentifier)
    }
}

final class InsufficientQuotaReporterTests: XCTestCase {
    private let domainA = NSFileProviderDomainIdentifier("domain-a")
    private let domainB = NSFileProviderDomainIdentifier("domain-b")

    override func setUp() async throws {
        try await super.setUp()
        // Reset shared dedup state between tests so order doesn't matter.
        await InsufficientQuotaReporter.clearSummaryDedup(domainIdentifier: domainA)
        await InsufficientQuotaReporter.clearSummaryDedup(domainIdentifier: domainB)
    }

    func testReportItemForwardsArguments() {
        let proxy = CapturingAppProxy()
        InsufficientQuotaReporter.reportItem(
            relativePath: "/folder/huge.bin",
            fileName: "huge.bin",
            fileBytes: 50_000_000,
            availableBytes: 1024,
            domainIdentifier: domainA,
            appProxy: proxy,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(proxy.capturedItems.count, 1)
        XCTAssertEqual(
            proxy.capturedItems.first,
            CapturingAppProxy.ItemCapture(
                relativePath: "/folder/huge.bin",
                fileName: "huge.bin",
                fileBytes: 50_000_000,
                availableBytes: 1024,
                domainIdentifier: "domain-a"
            )
        )
    }

    func testReportItemWithNilSizesPassesNilThrough() {
        let proxy = CapturingAppProxy()
        InsufficientQuotaReporter.reportItem(
            relativePath: "/x",
            fileName: "x",
            fileBytes: nil,
            availableBytes: nil,
            domainIdentifier: domainA,
            appProxy: proxy,
            log: FileProviderLogMock()
        )
        XCTAssertEqual(proxy.capturedItems.first?.fileBytes, nil)
        XCTAssertEqual(proxy.capturedItems.first?.availableBytes, nil)
    }

    func testReportItemWithoutProxyDropsSilently() {
        // No assertion crash, no exception, just an info-level log line.
        InsufficientQuotaReporter.reportItem(
            relativePath: "/x",
            fileName: "x",
            fileBytes: nil,
            availableBytes: nil,
            domainIdentifier: domainA,
            appProxy: nil,
            log: FileProviderLogMock()
        )
    }

    /// The summary is the per-folder activity entry that carries the "Retry all uploads"
    /// button. Multiple refusals in the same domain must NOT pile up summaries.
    func testSummaryDedupesPerDomain() async {
        let proxy = CapturingAppProxy()
        for _ in 0 ..< 5 {
            await InsufficientQuotaReporter.reportSummary(
                domainIdentifier: domainA,
                appProxy: proxy,
                log: FileProviderLogMock()
            )
        }
        XCTAssertEqual(proxy.capturedSummaryDomains, ["domain-a"])
    }

    func testSummaryRearmsAfterClear() async {
        let proxy = CapturingAppProxy()
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainA, appProxy: proxy, log: FileProviderLogMock()
        )
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainA, appProxy: proxy, log: FileProviderLogMock()
        )
        await InsufficientQuotaReporter.clearSummaryDedup(domainIdentifier: domainA)
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainA, appProxy: proxy, log: FileProviderLogMock()
        )
        XCTAssertEqual(proxy.capturedSummaryDomains, ["domain-a", "domain-a"])
    }

    func testSummaryDedupIsPerDomain() async {
        let proxy = CapturingAppProxy()
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainA, appProxy: proxy, log: FileProviderLogMock()
        )
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainB, appProxy: proxy, log: FileProviderLogMock()
        )
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainA, appProxy: proxy, log: FileProviderLogMock()
        )
        XCTAssertEqual(proxy.capturedSummaryDomains, ["domain-a", "domain-b"])
    }

    func testSummaryWithoutProxyDropsSilently() async {
        await InsufficientQuotaReporter.reportSummary(
            domainIdentifier: domainA, appProxy: nil, log: FileProviderLogMock()
        )
    }

    func testReportItemWithoutDomainDropsSilently() {
        let proxy = CapturingAppProxy()
        InsufficientQuotaReporter.reportItem(
            relativePath: "/x",
            fileName: "x",
            fileBytes: nil,
            availableBytes: nil,
            domainIdentifier: nil,
            appProxy: proxy,
            log: FileProviderLogMock()
        )
        XCTAssertTrue(proxy.capturedItems.isEmpty)
    }
}
