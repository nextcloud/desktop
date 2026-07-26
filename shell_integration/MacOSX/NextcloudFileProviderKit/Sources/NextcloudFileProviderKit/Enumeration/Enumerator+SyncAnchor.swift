//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// The version-tagged sync anchor scheme.
///
/// Every container the extension enumerates (working set, root, sub-directories, trash) hands the
/// framework an anchor of the form `"<extension-version>|<ISO8601-date>"`. On the next
/// `enumerateChanges(for:from:)` the embedded version is compared against the running extension; a
/// mismatch is rejected with `NSFileProviderError(.syncAnchorExpired)` so the framework drops its
/// cached `NSFileProviderItem` snapshots and re-enumerates the container. See nextcloud/desktop#10065.
///
extension Enumerator {
    ///
    /// Build a sync anchor that encodes the running extension bundle's `CFBundleShortVersionString` alongside the given timestamp.
    ///
    /// The same format is used for every container the extension enumerates (working set, root container, sub-directories, trash) so the framework's per-container persisted anchors all carry the version. On the next call to ``enumerateChanges(for:from:)`` the embedded version is compared against the running extension — any mismatch (including anchors persisted by builds older than this change, which carried only the ISO8601 date) is rejected with `NSFileProviderError(.syncAnchorExpired)`. That causes the framework to drop its cached sync state for that container and re-enumerate it via ``enumerateItems(for:startingAt:)``, so the fresh ``Item`` objects we hand back carry up-to-date `userInfo`, `contentPolicy`, and any other `NSFileProviderItem` properties whose derivation changed between app versions.
    ///
    /// See nextcloud/desktop#10065.
    ///
    public static func syncAnchor(at date: Date) -> NSFileProviderSyncAnchor {
        let raw = "\(currentExtensionVersion())|\(ISO8601DateFormatter().string(from: date))"
        // Force-unwrap is safe: an ASCII version string and an ISO8601 date both encode cleanly to UTF-8.
        return NSFileProviderSyncAnchor(raw.data(using: .utf8)!)
    }

    ///
    /// Parse a sync anchor produced by ``syncAnchor(at:)``.
    ///
    /// Returns `nil` for anchors that are not in the expected `"<version>|<ISO8601-date>"` format — including anchors persisted by builds older than #10065 that carried only the ISO8601 date. The caller treats `nil` as an expired anchor.
    ///
    static func parseSyncAnchor(_ anchor: NSFileProviderSyncAnchor) -> (version: String, date: Date)? {
        guard let raw = String(data: anchor.rawValue, encoding: .utf8) else {
            return nil
        }

        let parts = raw.split(separator: "|", maxSplits: 1, omittingEmptySubsequences: false)

        guard parts.count == 2 else {
            return nil
        }

        guard let date = ISO8601DateFormatter().date(from: String(parts[1])) else {
            return nil
        }

        return (String(parts[0]), date)
    }

    ///
    /// The running extension bundle's `CFBundleShortVersionString`, or the empty string when none is available — e.g. unit-test hosts without a versioned `Info.plist`.
    ///
    /// The empty-string fallback compares equal across calls inside the same process, so test anchors round-trip cleanly through ``syncAnchor(at:)`` and ``parseSyncAnchor(_:)`` without triggering the version-mismatch branch.
    ///
    static func currentExtensionVersion() -> String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
    }

    ///
    /// Validate the sync anchor the framework replayed before enumerating changes.
    ///
    /// Returns the embedded timestamp when the anchor is well-formed and its embedded extension
    /// version matches the running build. Otherwise reports `NSFileProviderError(.syncAnchorExpired)`
    /// to `observer` — so the framework drops cached snapshots and re-enumerates — and returns `nil`.
    /// See nextcloud/desktop#10065.
    ///
    func validatedSyncAnchorDate(
        _ anchor: NSFileProviderSyncAnchor, reportingTo observer: NSFileProviderChangeObserver
    ) -> Date? {
        guard let parsed = Self.parseSyncAnchor(anchor) else {
            logger.info("Sync anchor is not in the expected version-tagged format. Returning syncAnchorExpired so the framework re-enumerates this container and refreshes cached NSFileProviderItem snapshots. See nextcloud/desktop#10065.", [.item: enumeratedItemIdentifier])
            observer.finishEnumeratingWithError(NSFileProviderError(.syncAnchorExpired))
            return nil
        }

        let runningVersion = Self.currentExtensionVersion()

        guard parsed.version == runningVersion else {
            logger.info("Sync anchor's embedded extension version \"\(parsed.version)\" does not match the running extension version \"\(runningVersion)\". Returning syncAnchorExpired so the framework re-enumerates this container and refreshes cached NSFileProviderItem snapshots. See nextcloud/desktop#10065.", [.item: enumeratedItemIdentifier])
            observer.finishEnumeratingWithError(NSFileProviderError(.syncAnchorExpired))
            return nil
        }

        return parsed.date
    }
}
