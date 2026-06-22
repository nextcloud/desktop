//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
import NextcloudFileProviderXPC

///
/// Reports an upload that was refused due to insufficient server-side quota to the main app
/// over the existing extension-to-app XPC channel.
///
/// The main app surfaces TWO entries in the systray's activity view:
///
/// - One per refused item (subject "“X” was not synchronized" / message describing the quota
///   shortfall). Driven by `reportItem(...)`.
/// - One per affected domain, with a "Retry all uploads" primary button. Driven by
///   `reportSummary(...)` and deduped per-domain by this reporter so a 50-file paste produces
///   a single summary, not 50.
///
/// This mirrors the per-item activity entry that classic sync produces via
/// `User::slotAddErrorToGui` and the per-folder summary that classic sync produces via
/// `SyncEngine::slotInsufficientRemoteStorage` → `User::slotAddError(InsufficientRemoteStorage)`.
/// See nextcloud/desktop#9598.
///
enum InsufficientQuotaReporter {
    ///
    /// Per-domain dedup state for the summary report. Cleared by `clearSummaryDedup(...)` once
    /// any subsequent upload in the same domain succeeds, so the next quota event re-arms the
    /// summary message.
    ///
    private actor SummaryDedupState {
        static let shared = SummaryDedupState()

        private var reportedDomains = Set<String>()

        ///
        /// Atomically check-and-set: returns `true` if this is the first call for `domain`
        /// since the last reset (i.e. the caller should send the summary), `false` otherwise.
        ///
        func shouldReport(domain: String) -> Bool {
            guard !reportedDomains.contains(domain) else {
                return false
            }

            reportedDomains.insert(domain)
            return true
        }

        ///
        /// Clear dedup state for a domain so the next quota event for it re-emits the summary.
        ///
        func clear(domain: String) {
            reportedDomains.remove(domain)
        }
    }

    ///
    /// Send a per-item insufficient-quota report to the main app. Best-effort: errors are
    /// logged and never thrown.
    ///
    /// - Parameters:
    ///    - relativePath: The item's path relative to the file provider domain root.
    ///    - fileName: The display name of the item, e.g. `huge-video.mov`.
    ///    - fileBytes: Size of the file the user tried to upload, in bytes. `nil` if not known.
    ///    - availableBytes: Available server quota at the upload's parent, in bytes. `nil` if
    ///      not known (e.g. when the refusal came from a server 507 rather than a pre-flight
    ///      PROPFIND).
    ///    - domainIdentifier: The file provider domain identifier for the affected account.
    ///    - appProxy: The cached `id<AppProtocol>` proxy held by the running
    ///      `FileProviderExtension`. May be `nil` (e.g. main app not running) — in which case
    ///      the report is dropped silently.
    ///    - log: The logger used for diagnostic output.
    ///
    static func reportItem(relativePath: String, fileName: String, fileBytes: Int64?, availableBytes: Int64?, domainIdentifier: NSFileProviderDomainIdentifier?, appProxy: (any AppProtocol)?, log: any FileProviderLogging) {
        let logger = FileProviderLogger(category: "InsufficientQuotaReporter", log: log)

        guard let appProxy else {
            logger.info("Quota item report dropped because of missing XPC proxy.", [.name: fileName])
            return
        }

        guard let domainIdentifier else {
            logger.info("Quota item report dropped because of missing domain identifier.", [.name: fileName])
            return
        }

        logger.info("Reporting insufficient quota item to main app.", [.name: fileName])

        appProxy.reportInsufficientQuota(forItem: relativePath, fileName: fileName, fileBytes: fileBytes.map { NSNumber(value: $0) }, availableBytes: availableBytes.map { NSNumber(value: $0) }, forDomainIdentifier: domainIdentifier.rawValue)
    }

    ///
    /// Send a per-domain insufficient-quota *summary* report to the main app, at most once per
    /// domain until `clearSummaryDedup(...)` is called. Safe to call repeatedly during a burst
    /// of quota refusals.
    ///
    /// - Parameters:
    ///   - domainIdentifier: The file provider domain identifier for the affected account.
    ///   - appProxy: The cached `id<AppProtocol>` proxy. May be `nil`; the report is dropped
    ///     silently in that case.
    ///   - log: The logger used for diagnostic output.
    ///
    static func reportSummary(domainIdentifier: NSFileProviderDomainIdentifier?, appProxy: (any AppProtocol)?, log: any FileProviderLogging) async {
        let logger = FileProviderLogger(category: "InsufficientQuotaReporter", log: log)

        guard let appProxy else {
            logger.info("Quota summary report dropped because of missing XPC proxy.")
            return
        }

        guard let domainIdentifier else {
            logger.info("Quota summary report dropped because of missing domain identifier.")
            return
        }

        let shouldReport = await SummaryDedupState.shared.shouldReport(domain: domainIdentifier.rawValue)

        guard shouldReport else {
            logger.debug("Quota summary already reported for domain.", [.name: domainIdentifier.rawValue])
            return
        }

        logger.info("Reporting insufficient quota summary to main app.", [.name: domainIdentifier.rawValue])
        appProxy.reportInsufficientQuotaSummary(forDomainIdentifier: domainIdentifier.rawValue)
    }

    ///
    /// Reset summary dedup state for the given domain. Called from upload-success paths so
    /// the next quota event re-arms the summary entry.
    ///
    static func clearSummaryDedup(domainIdentifier: NSFileProviderDomainIdentifier?) async {
        guard let domainIdentifier else {
            return
        }

        await SummaryDedupState.shared.clear(domain: domainIdentifier.rawValue)
    }
}
