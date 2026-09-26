//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudFileProviderXPC

///
/// Reports an item that the extension refused to synchronize to the main app over the existing
/// XPC channel.
///
/// The main app surfaces the report in the systray's activity view — the same surface used by the
/// classic sync engine for excluded items. See `AppProtocol.reportItemExcludedFromSync(...)` for
/// the wire-level contract.
///
enum ItemExclusionReporter {
    enum Reason {
        case bundle
        case excludedDestination
        case remoteDeletionFailed

        var localizedText: String {
            switch self {
                case .bundle:
                    NSLocalizedString(
                        "BundleExclusion.Reason",
                        bundle: .module,
                        value: "macOS bundles cannot be synchronized yet — they remain on this Mac only.",
                        comment: "Activity-view explanation when a macOS bundle is refused by the file provider extension."
                    )
                case .excludedDestination:
                    NSLocalizedString(
                        "ExcludedDestination.Reason",
                        bundle: .module,
                        value: "This item was moved into a destination excluded from synchronization — it remains on this Mac only.",
                        comment: "Activity-view explanation when an item is moved into a folder excluded from synchronization."
                    )
                case .remoteDeletionFailed:
                    NSLocalizedString(
                        "ExcludedDestination.RemoteDeletionFailedReason",
                        bundle: .module,
                        value: "This item was moved into a destination excluded from synchronization, but its remote copy could not be removed.",
                        comment: "Activity-view explanation when the remote copy of an item moved into an excluded folder could not be removed."
                    )
            }
        }
    }

    ///
    /// Localized human-readable reason shown to the user in the activity view.
    ///
    static func reasonText(for reason: Reason = .bundle) -> String {
        reason.localizedText
    }

    ///
    /// Send a one-shot report to the main app over XPC. Best-effort: errors are logged at `info`
    /// and never thrown.
    ///
    /// - Parameters:
    ///   - relativePath: The item's path relative to the file provider domain root.
    ///   - fileName: The display name of the item, e.g. `PluginKit Monitor.app`.
    ///   - reason: The localized explanation shown in the activity view.
    ///   - domainIdentifier: The file provider domain identifier for the affected account.
    ///   - appProxy: The cached `id<AppProtocol>` proxy held by the running `FileProviderExtension`. May be `nil` (e.g. main app not running) — in which case the report is dropped silently.
    ///   - log: The logger used for diagnostic output.
    ///
    static func report(
        relativePath: String,
        fileName: String,
        reason: Reason = .bundle,
        domainIdentifier: NSFileProviderDomainIdentifier,
        appProxy: (any AppProtocol)?,
        log: any FileProviderLogging
    ) {
        let logger = FileProviderLogger(category: "ItemExclusionReporter", log: log)

        guard let appProxy else {
            logger.info("No XPC proxy to the main app — exclusion report dropped.", [.name: fileName])
            return
        }

        logger.info("Reporting item exclusion to main app via XPC.", [.name: fileName])

        appProxy.reportItemExcluded(
            fromSync: relativePath,
            fileName: fileName,
            reason: reasonText(for: reason),
            forDomainIdentifier: domainIdentifier.rawValue
        )
    }
}
