//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudFileProviderXPC

///
/// Reports a bundle-shaped item that the extension refused to sync to the main app over the
/// existing XPC channel.
///
/// The main app surfaces the report in the systray's activity view — the same surface used by
/// the classic sync engine for excluded items. See `AppProtocol.reportItemExcludedFromSync(...)`
/// for the wire-level contract.
///
/// macOS bundles (`.app`, `.key`, `.pages`, `.fcpbundle`, …) are folders the system presents as
/// atomic files. WebDAV cannot represent the full bundle layout (symlinks, resource forks,
/// permissions), so until a proper transport is in place, every bundle is excluded at the file
/// provider boundary and the user is informed politely instead of being left with a partial
/// sync.
///
enum BundleExclusionReporter {
    ///
    /// Localized human-readable reason shown to the user in the activity view.
    ///
    static func reasonText() -> String {
        NSLocalizedString(
            "BundleExclusion.Reason",
            bundle: .module,
            value: "macOS bundles cannot be synchronized yet — they remain on this Mac only.",
            comment: "Activity-view explanation when a macOS bundle is refused by the file provider extension."
        )
    }

    ///
    /// Send a one-shot report to the main app over XPC. Best-effort: errors are logged at `info`
    /// and never thrown.
    ///
    /// - Parameters:
    ///   - relativePath: The item's path relative to the file provider domain root.
    ///   - fileName: The display name of the item, e.g. `PluginKit Monitor.app`.
    ///   - domainIdentifier: The file provider domain identifier for the affected account.
    ///   - appProxy: The cached `id<AppProtocol>` proxy held by the running `FileProviderExtension`. May be `nil` (e.g. main app not running) — in which case the report is dropped silently.
    ///   - log: The logger used for diagnostic output.
    ///
    static func report(
        relativePath: String,
        fileName: String,
        domainIdentifier: NSFileProviderDomainIdentifier,
        appProxy: (any AppProtocol)?,
        log: any FileProviderLogging
    ) {
        let logger = FileProviderLogger(category: "BundleExclusionReporter", log: log)

        guard let appProxy else {
            logger.info("No XPC proxy to the main app — bundle-exclusion report dropped.", [.name: fileName])
            return
        }

        logger.info("Reporting bundle exclusion to main app via XPC.", [.name: fileName])

        appProxy.reportItemExcluded(
            fromSync: relativePath,
            fileName: fileName,
            reason: reasonText(),
            forDomainIdentifier: domainIdentifier.rawValue
        )
    }
}
