//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit
import UniformTypeIdentifiers

///
/// Data model implementation for file provider items as defined by the file provider framework and `NSFileProviderItemProtocol`.
///
public final class Item: NSObject, NSFileProviderItem, Sendable {
    public enum FileProviderItemTransferError: Error {
        case downloadError
        case uploadError
    }

    public let dbManager: FilesDatabaseManager
    public let metadata: SendableItemMetadata
    public let parentItemIdentifier: NSFileProviderItemIdentifier
    public let account: Account
    public let remoteInterface: RemoteInterface

    private let displayFileActions: Bool
    private let remoteSupportsTrash: Bool

    public var itemIdentifier: NSFileProviderItemIdentifier {
        NSFileProviderItemIdentifier(metadata.ocId)
    }

    public var capabilities: NSFileProviderItemCapabilities {
        var capabilities: NSFileProviderItemCapabilities = []
        let permissions = metadata.permissions.uppercased()

        if permissions.contains("G"), metadata.directory { // Readable
            capabilities.insert(.allowsContentEnumerating)
        } else if permissions.contains("G") {
            capabilities.insert(.allowsReading)
        }

        if metadata.lock == false || (metadata.lock == true && metadata.lockOwnerType == NKLockType.token.rawValue && metadata.ownerId == metadata.lockOwner && metadata.lockToken != nil) {
            if permissions.contains("D") { // Deletable
                capabilities.insert(.allowsDeleting)
            }

            if remoteSupportsTrash, !isLockFileName(filename) {
                capabilities.insert(.allowsTrashing)
            }

            if permissions.contains("W"), !metadata.directory { // Updateable (file)
                capabilities.insert(.allowsWriting)
            }

            if permissions.contains("NV") { // Updateable, renameable, moveable
                capabilities.formUnion([.allowsRenaming, .allowsReparenting])

                if metadata.directory {
                    capabilities.insert(.allowsAddingSubItems)
                }
            }

            if permissions.contains("CK"), metadata.directory { // Folder not changeable but adding sub-files & -folders
                capabilities.insert(.allowsWriting)
            }
        }

        capabilities.insert(.allowsExcludingFromSync)

        return capabilities
    }

    public var itemVersion: NSFileProviderItemVersion {
        // `metadataVersion` embeds the running extension's `CFBundleShortVersionString`
        // alongside the server etag so the framework's cached snapshot is detected as stale
        // after any extension update that changes how `userInfo` / `contentPolicy` /
        // `fileSystemFlags` / etc. are derived from `metadata`. Without the extension-version
        // component, an item whose server etag is unchanged would carry the same
        // `metadataVersion` we previously handed the framework, the framework would treat
        // its cached snapshot as still valid, and the new derivations would never reach the
        // system — leaving e.g. the `displayOpenInBrowser` / `displayCopyInternalLink`
        // userInfo keys missing on items enumerated by older builds. `contentVersion` stays
        // bare-etag because the file content itself didn't change across the upgrade and
        // bumping it would force the framework to re-download every materialised file.
        //
        // See nextcloud/desktop#10065.
        let extensionVersion = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
        var metadataVersionString = "\(metadata.etag)|\(extensionVersion)"

        // A directory's `displayEvictDescendants` ("Remove downloaded items")
        // depends on whether it holds an evictable descendant file — state that
        // lives outside the folder's own etag. Fold that boolean into
        // `metadataVersion` so the framework detects its cached snapshot as stale
        // and re-reads the recomputed `userInfo` when a descendant materializes or
        // is evicted. Without this, the ancestor-refresh nudge (see
        // `FileProviderExtension.materializedItemsDidChange`) would hand back an
        // unchanged version and the framework would drop the update — the same
        // versioning rationale as the extension-version component above (#10085,
        // #10065). A bool suffices: it flips exactly at the 0↔≥1 boundary where
        // the folder action itself flips, so it does not over-invalidate on every
        // descendant change.
        if metadata.directory {
            metadataVersionString += "|\(dbManager.hasEvictableDescendantFile(directoryMetadata: metadata))"
        }

        return NSFileProviderItemVersion(contentVersion: metadata.etag.data(using: .utf8)!, metadataVersion: metadataVersionString.data(using: .utf8)!)
    }

    public var filename: String {
        metadata.isTrashed && !metadata.trashbinFileName.isEmpty ?
            metadata.trashbinFileName : !metadata.fileName.isEmpty ?
            metadata.fileName : "unnamed file"
    }

    public var contentType: UTType {
        if itemIdentifier == .rootContainer || (metadata.contentType.isEmpty && metadata.directory) {
            return .folder
        } else if metadata.contentType == "httpd/unix-directory", metadata.directory {
            let filenameComponents = filename.components(separatedBy: ".")

            if filenameComponents.count > 1, let ext = filenameComponents.last {
                return UTType(filenameExtension: ext, conformingTo: .directory) ?? .folder
            }

            return .folder
        } else if !metadata.contentType.isEmpty, let type = UTType(metadata.contentType) {
            return type
        }

        let filenameExtension = filename.components(separatedBy: ".").last ?? ""

        return UTType(filenameExtension: filenameExtension) ?? .content
    }

    public var documentSize: NSNumber? {
        NSNumber(value: metadata.size)
    }

    public var creationDate: Date? {
        metadata.creationDate as Date
    }

    public var lastUsedDate: Date? {
        metadata.date as Date
    }

    public var contentModificationDate: Date? {
        metadata.date as Date
    }

    public var isDownloaded: Bool {
        metadata.directory || metadata.downloaded
    }

    public var isDownloading: Bool {
        metadata.isDownload
    }

    public var downloadingError: Error? {
        if metadata.status == Status.downloadError.rawValue {
            return FileProviderItemTransferError.downloadError
        }
        return nil
    }

    public var isUploaded: Bool {
        metadata.uploaded
    }

    public var isUploading: Bool {
        metadata.isUpload
    }

    public var uploadingError: Error? {
        if metadata.status == Status.uploadError.rawValue {
            FileProviderItemTransferError.uploadError
        } else {
            nil
        }
    }

    public var isShared: Bool {
        false // !metadata.shareType.isEmpty // Interim solution to counteract Finder misleadingly displaying shared items with an iCloud branded banner.
    }

    public var isSharedByCurrentUser: Bool {
        false // isShared && metadata.ownerId == account.id // Interim solution to counteract Finder misleadingly displaying shared items with an iCloud branded banner.
    }

    public var ownerNameComponents: PersonNameComponents? {
        guard isShared, !isSharedByCurrentUser else { return nil }
        let formatter = PersonNameComponentsFormatter()
        return formatter.personNameComponents(from: metadata.ownerDisplayName)
    }

    public var childItemCount: NSNumber? {
        if metadata.directory {
            NSNumber(integerLiteral: dbManager.childItemCount(directoryMetadata: metadata))
        } else {
            nil
        }
    }

    public var fileSystemFlags: NSFileProviderFileSystemFlags {
        if metadata.isLockFileOfLocalOrigin {
            return [
                .hidden,
                .userReadable,
                .userWritable
            ]
        }

        if metadata.lock, metadata.lockOwnerType != NKLockType.user.rawValue || metadata.lockOwner != account.username, metadata.lockTimeOut ?? Date() > Date() {
            return [
                .userReadable
            ]
        }

        return [
            .userReadable,
            .userWritable
        ]
    }

    public var userInfo: [AnyHashable: Any]? {
        var userInfoDict = [AnyHashable: Any]()
        userInfoDict["displayFileActions"] = displayFileActions

        if metadata.lock {
            // Can be used to display lock/unlock context menu entries for FPUIActions
            // Note that only files, not folders, should be lockable/unlockable
            userInfoDict["locked"] = metadata.lock
        }

        userInfoDict["displayKeepDownloaded"] = !metadata.keepDownloaded
        userInfoDict["displayAllowAutoEvicting"] = metadata.keepDownloaded
        // "Remove download" is split into two actions that share the same evict
        // handler but carry different, statically-declared labels (#10085):
        //
        // - `displayEvict` → "Remove download", for a *file* whose own payload is
        //   materialized. Files carry that state in `downloaded`.
        // - `displayEvictDescendants` → "Remove downloaded items", for a *folder*
        //   (including the root) that holds at least one *evictable* descendant
        //   file: downloaded, not pinned via "Always keep downloaded" (an
        //   individually-pinned descendant is not removable and can't be evicted —
        //   -2008 NonEvictable, #9891). Folders never get `downloaded == true`
        //   during sync, and the file label would misdescribe a folder. Sub-folders
        //   alone do not count; unlike the sticky `visitedDirectory`, this clears
        //   itself after eviction because it tracks descendants' `downloaded` flags,
        //   which the materialized-set observer resets.
        //
        // Both are restricted to non-pinned items so the action only appears once
        // the framework has refreshed `contentPolicy` to `.inherited`. This field
        // and `contentPolicy` are read from the same `Item` returned by
        // `item(for:)`, so they always agree — preventing the -2008 NonEvictable
        // race that would otherwise occur if we tried to evict while
        // `requestModification`'s unpin signal was still queued (#9891). Pinning a
        // folder recursively sets `keepDownloaded` on the folder itself, so the
        // same guard keeps the folder action hidden until the two-step unpin flow.
        let notPinned = !metadata.keepDownloaded
        if metadata.directory {
            userInfoDict["displayEvict"] = false
            userInfoDict["displayEvictDescendants"] = dbManager.hasEvictableDescendantFile(directoryMetadata: metadata) && notPinned
        } else {
            userInfoDict["displayEvict"] = metadata.downloaded && notPinned
            userInfoDict["displayEvictDescendants"] = false
        }

        // Gate the "Open in browser" context menu action on items that have a
        // server-side counterpart whose private link the main app can resolve.
        // This excludes:
        // - the root and trash pseudo-containers (no fileId, no web page),
        // - lock files of local origin (placeholders not present on the server),
        // - ignored items and freshly-created items that have not been
        //   uploaded yet (`uploaded == false` ⇒ no server-side record).
        // The numeric `fileId` check is defensive — a successfully uploaded
        // item should always carry one, and `fetchPrivateLinkUrl` requires it
        // for the deprecated fallback URL. See nextcloud/desktop#10025.
        userInfoDict["displayOpenInBrowser"] = ![.rootContainer, .trashContainer].contains(itemIdentifier)
            && !metadata.isLockFileOfLocalOrigin
            && metadata.uploaded
            && !metadata.fileId.isEmpty

        // Gate the "Copy internal link" context menu action on the same
        // preconditions as "Open in browser": both actions resolve the same
        // per-item private link via PROPFIND (with the deprecated
        // `/index.php/f/<fileId>` URL as fallback), so they share server-side
        // requirements. A dedicated flag — rather than reusing
        // `displayOpenInBrowser` — keeps future divergence (e.g. excluding
        // certain item types from the clipboard but not the browser, or vice
        // versa) a one-line change. See nextcloud/desktop#10024.
        userInfoDict["displayCopyInternalLink"] = ![.rootContainer, .trashContainer].contains(itemIdentifier)
            && !metadata.isLockFileOfLocalOrigin
            && metadata.uploaded
            && !metadata.fileId.isEmpty

        // https://docs.nextcloud.com/server/latest/developer_manual/client_apis/WebDAV/basic.html
        if metadata.permissions.uppercased().contains("R") /* Shareable */, ![.rootContainer, .trashContainer].contains(itemIdentifier) {
            userInfoDict["displayShare"] = true
        }

        return userInfoDict
    }

    public var contentPolicy: NSFileProviderContentPolicy {
        if metadata.keepDownloaded {
            return .downloadEagerlyAndKeepDownloaded
        }

        return .inherited
    }

    public var keepDownloaded: Bool {
        metadata.keepDownloaded
    }

    ///
    /// Factory method to create a root container item.
    ///
    /// - Returns: A file provider item for the root container of the given account.
    ///
    public static func rootContainer(
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        remoteSupportsTrash: Bool,
        log: any FileProviderLogging
    ) -> Item {
        var metadata = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.rootContainer.rawValue,
            account: account.ncKitAccount,
            classFile: NKTypeClassFile.directory.rawValue,
            contentType: "", // Placeholder as not set in original code
            creationDate: Date(), // Default as not set in original code
            directory: true,
            e2eEncrypted: false, // Default as not set in original code
            etag: "", // Placeholder as not set in original code
            fileId: "", // Placeholder as not set in original code
            fileName: "/",
            fileNameView: "/",
            hasPreview: false, // Default as not set in original code
            iconName: "", // Placeholder as not set in original code
            mountType: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            path: "", // Placeholder as not set in original code
            serverUrl: account.davFilesUrl,
            size: 0, // Default as not set in original code
            uploaded: true,
            urlBase: "", // Placeholder as not set in original code
            user: "", // Placeholder as not set in original code
            userId: "" // Placeholder as not set in original code
        )

        // Merge persisted state for the root from the database so that
        // per-item toggles (most importantly `keepDownloaded`) survive across
        // calls to this factory. Without this, the root is always rebuilt
        // with defaults and `userInfo` keeps offering "Always keep downloaded"
        // even after the user has enabled it — `displayKeepDownloaded` /
        // `displayAllowAutoEvicting` and `contentPolicy` all derive from the
        // freshly-synthesised (and therefore stale) metadata.
        if let existing = dbManager.itemMetadata(ocId: metadata.ocId) {
            metadata.keepDownloaded = existing.keepDownloaded
            metadata.downloaded = existing.downloaded
        }

        return Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: false,
            remoteSupportsTrash: remoteSupportsTrash,
            log: log
        )
    }

    public static func trashContainer(
        remoteInterface: RemoteInterface,
        account: Account,
        dbManager: FilesDatabaseManager,
        remoteSupportsTrash: Bool,
        log: any FileProviderLogging
    ) -> Item {
        var metadata = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.trashContainer.rawValue,
            account: account.ncKitAccount,
            classFile: NKTypeClassFile.directory.rawValue,
            contentType: "", // Placeholder as not set in original code
            creationDate: Date(), // Default as not set in original code
            directory: true,
            e2eEncrypted: false, // Default as not set in original code
            etag: "", // Placeholder as not set in original code
            fileId: "", // Placeholder as not set in original code
            fileName: "Trash",
            fileNameView: "Trash",
            hasPreview: false, // Default as not set in original code
            iconName: "", // Placeholder as not set in original code
            mountType: "", // Placeholder as not set in original code
            ownerId: "", // Placeholder as not set in original code
            ownerDisplayName: "", // Placeholder as not set in original code
            path: "", // Placeholder as not set in original code
            serverUrl: account.trashUrl,
            size: 0, // Default as not set in original code
            uploaded: true,
            urlBase: "", // Placeholder as not set in original code
            user: "", // Placeholder as not set in original code
            userId: "" // Placeholder as not set in original code
        )

        // See the matching rationale in `rootContainer(...)`: merge persisted
        // per-item toggles from the database so the trash container does not
        // forget its state between factory invocations.
        if let existing = dbManager.itemMetadata(ocId: metadata.ocId) {
            metadata.keepDownloaded = existing.keepDownloaded
            metadata.downloaded = existing.downloaded
        }

        return Item(
            metadata: metadata,
            parentItemIdentifier: .trashContainer,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: false,
            remoteSupportsTrash: remoteSupportsTrash,
            log: log
        )
    }

    let logger: FileProviderLogger

    public required init(
        metadata: SendableItemMetadata,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        displayFileActions: Bool,
        remoteSupportsTrash: Bool,
        log: any FileProviderLogging
    ) {
        self.metadata = metadata
        self.parentItemIdentifier = parentItemIdentifier
        self.account = account
        logger = FileProviderLogger(category: "Item", log: log)
        self.remoteInterface = remoteInterface
        self.dbManager = dbManager
        self.displayFileActions = displayFileActions
        self.remoteSupportsTrash = remoteSupportsTrash
        super.init()
    }

    public static func storedItem(identifier: NSFileProviderItemIdentifier, account: Account, remoteInterface: RemoteInterface, dbManager: FilesDatabaseManager, log: any FileProviderLogging) async -> Item? {
        // resolve the given identifier to a record in the model

        let remoteSupportsTrash = await remoteInterface.supportsTrash(account: account)

        guard identifier != .rootContainer else {
            return Item.rootContainer(
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                remoteSupportsTrash: remoteSupportsTrash,
                log: log
            )
        }
        guard identifier != .trashContainer else {
            return Item.trashContainer(
                remoteInterface: remoteInterface,
                account: account,
                dbManager: dbManager,
                remoteSupportsTrash: remoteSupportsTrash,
                log: log
            )
        }

        guard let metadata = dbManager.itemMetadata(identifier) else {
            return nil
        }

        let parentItemIdentifier: NSFileProviderItemIdentifier? = if metadata.isTrashed {
            .trashContainer
        } else {
            await dbManager.parentItemIdentifierWithRemoteFallback(
                fromMetadata: metadata,
                remoteInterface: remoteInterface,
                account: account
            )
        }

        guard let parentItemIdentifier else {
            return nil
        }

        // Display File Actions

        let displayFileActions = await Item.typeHasApplicableContextMenuItems(account: account, remoteInterface: remoteInterface, candidate: metadata.contentType)

        return Item(
            metadata: metadata,
            parentItemIdentifier: parentItemIdentifier,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            displayFileActions: displayFileActions,
            remoteSupportsTrash: remoteSupportsTrash,
            log: log
        )
    }

    public func localUrlForContents(domain: NSFileProviderDomain) async -> URL? {
        guard isDownloaded else {
            logger.error("Unable to get local URL for item contents. Item is not materialised.", [.name: filename])

            return nil
        }

        guard let manager = NSFileProviderManager(for: domain), let fileUrl = try? await manager.getUserVisibleURL(for: itemIdentifier) else {
            logger.error("Unable to get manager or user visible url for item. Cannot provide local URL for contents.", [.name: filename])

            return nil
        }

        let fm = FileManager.default
        let tempLocation = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let coordinator = NSFileCoordinator()
        var readData: Data?

        coordinator.coordinate(readingItemAt: fileUrl, options: [], error: nil) { readURL in
            readData = try? Data(contentsOf: readURL)
        }

        guard let readData else {
            return nil
        }

        do {
            try readData.write(to: tempLocation)
        } catch {
            logger.error("Unable to write file item contents to temporary URL.", [.name: filename, .error: error])
        }

        return tempLocation
    }
}
