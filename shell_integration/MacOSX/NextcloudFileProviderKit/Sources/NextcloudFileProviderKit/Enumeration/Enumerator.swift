//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

///
/// The `NSFileProviderEnumerator` implementation that enumerates file-provider items and derives
/// remote change sets for one container.
///
/// An instance enumerates exactly one of three kinds of container, distinguished by
/// ``enumeratedItemIdentifier``:
///
/// - **Working set** (`.workingSet`): the items the system keeps track of — visited folders and
///   downloaded files. This is the only container a change notification ever signals (the app calls
///   `signalEnumerator(for: .workingSet)`), so working-set change enumeration is what drives remote
///   updates into the framework. See ``scanMaterialisedItemsForRemoteChanges()``.
/// - **Trash** (`.trashContainer`): items deleted on this device. See the `Trash` extension.
/// - **Regular directory**: a normal folder, read from the server and (for changes) diffed against
///   the local database. See the `ItemEnumeration` and `ChangeEnumeration` extensions.
///
/// The framework drives two distinct operations, kept on separate code paths:
///
/// - **Items** (``enumerateItems(for:startingAt:)``): the full current contents of the container,
///   paginated when the server supports it.
/// - **Changes** (``enumerateChanges(for:from:)``): only what changed since a sync anchor, reported
///   as creations/updates/deletions via a ``ChangeSet``.
///
/// Each public protocol method below is a thin dispatcher that validates input and routes to the
/// per-container-kind implementation living in a focused `Enumerator+*.swift` extension. The shared
/// remote read returns a ``RemoteReadResult``; the version-tagged sync anchor scheme lives in the
/// `SyncAnchor` extension; turning metadata into observer callbacks lives in `ObserverReporting`.
///
public final class Enumerator: NSObject, NSFileProviderEnumerator, Sendable {
    let enumeratedItemIdentifier: NSFileProviderItemIdentifier
    /// Internal (not private) so the concern-specific extensions in sibling files can read it.
    let enumeratedItemMetadata: SendableItemMetadata?

    private var enumeratingSystemIdentifier: Bool {
        Self.isSystemIdentifier(enumeratedItemIdentifier)
    }

    let domain: NSFileProviderDomain?
    let dbManager: FilesDatabaseManager

    // Internal (not private) so the change-enumeration extensions in sibling files can read these.
    let currentAnchor = Enumerator.syncAnchor(at: Date())
    let pageItemCount: Int
    let logger: FileProviderLogger
    let account: Account
    let remoteInterface: RemoteInterface
    let serverUrl: String

    /// Holds the undelivered remainder of a large change set so it can be reported one batch at a time
    /// across the framework's `moreComing` re-invocations without re-running the destructive derivation.
    /// See ``ChangeDeliveryBuffer`` and ``drainChangeBuffer(for:startAnchor:finalAnchor:suggested:)``.
    ///
    /// Load-bearing assumption: the framework continues a `moreComing` chain on the *same* enumerator
    /// instance, re-invoking `enumerateChanges(for:from:)` with the anchor the previous batch returned
    /// (see `NSFileProviderEnumerating.h`, `currentSyncAnchorWithCompletionHandler`). The in-memory
    /// buffer therefore survives the chain. If the extension process is recycled mid-drain, a fresh
    /// instance re-derives from the original anchor; see ``ChangeDeliveryBuffer`` for the bounded
    /// created/updated loss this can cause for change sets larger than ``maxBatchSize``.
    let changeBuffer: ChangeDeliveryBuffer

    /// Default number of items reported per page/batch when the system does not suggest one. Mirrors the
    /// per-page item budget used for paginated item enumeration (``pageItemCount``).
    static let defaultBatchSize = 1000

    /// Hard ceiling on the items reported in a single page/batch. The framework asserts past 100× the
    /// system's suggested size (≈20000 for the default suggestion of 200, which is the limit the
    /// `__FILEPROVIDER_OBSERVER_TOO_MANY_ITEMS__` crash hit); staying well under that avoids the assertion
    /// while keeping batch counts low. It also leaves headroom for the single recovered-parent item that
    /// invalid-parent recovery may prepend to a batch.
    static let maxBatchSize = 4000

    private static func isSystemIdentifier(_ identifier: NSFileProviderItemIdentifier) -> Bool {
        identifier == .rootContainer || identifier == .trashContainer || identifier == .workingSet
    }

    public init(
        enumeratedItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        pageSize: Int = 1000,
        log: any FileProviderLogging
    ) throws {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        self.remoteInterface = remoteInterface
        self.account = account
        self.dbManager = dbManager
        self.domain = domain
        pageItemCount = pageSize
        logger = FileProviderLogger(category: "Enumerator", log: log)
        changeBuffer = ChangeDeliveryBuffer(log: log)

        if Self.isSystemIdentifier(enumeratedItemIdentifier) {
            logger.info("Providing enumerator for a system defined container.", [.item: enumeratedItemIdentifier])
            serverUrl = account.davFilesUrl
            enumeratedItemMetadata = nil
        } else {
            enumeratedItemMetadata = dbManager.itemMetadata(enumeratedItemIdentifier)

            guard let enumeratedItemMetadata, enumeratedItemMetadata.deleted == false else {
                logger.error("Could not find item with identifier.", [.item: enumeratedItemIdentifier])
                throw NSFileProviderError(.noSuchItem)
            }

            logger.debug("Providing enumerator for item with identifier.", [.item: enumeratedItemIdentifier, .name: enumeratedItemMetadata.fileName])
            serverUrl = enumeratedItemMetadata.serverUrl + "/" + enumeratedItemMetadata.fileName
        }

        logger.info("Set up enumerator.", [.account: self.account.ncKitAccount, .url: serverUrl])
        super.init()
    }

    public func invalidate() {
        logger.debug("Enumerator is being invalidated.", [.item: enumeratedItemIdentifier, .name: enumeratedItemMetadata?.fileName])
    }

    ///
    /// Resolve how many items to report per page/batch from the observer's system-set suggestion,
    /// falling back to ``defaultBatchSize`` when the system did not set one and clamping to
    /// ``maxBatchSize``.
    ///
    /// `suggestedBatchSize` / `suggestedPageSize` are optional observer properties the system sets to the
    /// value best suited to the current enumeration; they surface as `nil` (or `0`) when unset.
    ///
    func effectiveBatchSize(suggested: Int?) -> Int {
        let base = suggested.flatMap { $0 > 0 ? $0 : nil } ?? Self.defaultBatchSize
        return min(base, Self.maxBatchSize)
    }

    // MARK: - Protocol methods

    public func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        logger.info("Received enumerate items request for enumerator with user", [.account: account.ncKitAccount, .url: serverUrl])

        if enumeratedItemIdentifier == .trashContainer {
            enumerateTrashItems(for: observer)
        } else if enumeratedItemIdentifier == .workingSet {
            enumerateWorkingSetItems(for: observer, startingAt: page)
        } else {
            enumerateContainerItems(for: observer, startingAt: page)
        }
    }

    public func enumerateChanges(for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor) {
        logger.debug("Enumerating changes (anchor: \(String(data: anchor.rawValue, encoding: .utf8) ?? "")).", [.url: serverUrl])

        // The version-tagged anchor check applies to every container. An anchor that does not parse,
        // or whose embedded extension version differs from the running build, is rejected as expired
        // so the framework drops its cached snapshots and re-enumerates. See nextcloud/desktop#10065.
        guard let anchorDate = validatedSyncAnchorDate(anchor, reportingTo: observer) else {
            return
        }

        if enumeratedItemIdentifier == .workingSet {
            enumerateWorkingSetChanges(for: observer, since: anchorDate, anchor: anchor)
        } else if enumeratedItemIdentifier == .trashContainer {
            enumerateTrashChanges(for: observer, anchor: anchor)
        } else {
            enumerateContainerChanges(for: observer, anchor: anchor)
        }
    }

    public func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(currentAnchor)
    }
}
