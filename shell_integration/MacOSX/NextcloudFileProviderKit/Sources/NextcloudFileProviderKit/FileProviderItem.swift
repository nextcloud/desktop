//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import UniformTypeIdentifiers

///
/// The data model for file provider items of this extension as required by the file provider framework.
///
/// To enable thread-safety, this is a dedicated type and cannot be based on the data model entities.
///
public final class FileProviderItem: NSObject, Sendable, NSFileProviderItem {
    ///
    /// The server-side unique identifier for the item.
    ///
    public let ocID: String?

    // MARK: - Stored NSFileProviderItem Properties

    public let capabilities: NSFileProviderItemCapabilities
    public let childItemCount: NSNumber?
    public let contentModificationDate: Date?

    ///
    /// Value storing property for ``contentPolicy``.
    ///
    private let storedContentPolicy: Int

    ///
    /// Because availability attributes cannot be applied to stored properties, this has to be implemented through a computed property based on a stored property with a scalar type.
    ///
    /// - Returns: `.inherited` as the default, if no other value is specified or available.
    ///
    @available(macOS 13.0, *)
    public var contentPolicy: NSFileProviderContentPolicy {
        guard let storedContentPolicy = NSFileProviderContentPolicy(rawValue: storedContentPolicy) else {
            return .inherited
        }

        return storedContentPolicy
    }

    public let contentType: UTType
    public let creationDate: Date?
    public let documentSize: NSNumber?
    public let downloadingError: (any Error)?
    public let filename: String
    public let fileSystemFlags: NSFileProviderFileSystemFlags
    public let isDownloaded: Bool
    public let isDownloading: Bool
    public let isMostRecentVersionDownloaded: Bool
    public let isShared: Bool
    public let isSharedByCurrentUser: Bool
    public let isUploaded: Bool
    public let isUploading: Bool
    public let itemIdentifier: NSFileProviderItemIdentifier

    ///
    /// `nonisolated(unsafe)` is necessary because some parts of the file provider framework have not been updated with full concurrency annotations yet.
    /// It is safe to use here, though, because it is a `let` defined in the initializer.
    ///
    public nonisolated(unsafe) let itemVersion: NSFileProviderItemVersion
    public let lastUsedDate: Date?
    public let mostRecentEditorNameComponents: PersonNameComponents?
    public let ownerNameComponents: PersonNameComponents?
    public let parentItemIdentifier: NSFileProviderItemIdentifier
    public let uploadingError: (any Error)?

    ///
    /// `nonisolated(unsafe)` is necessary because some parts of the file provider framework have not been updated with full concurrency annotations yet.
    /// It is safe to use here, though, because it is a `let` defined in the initializer.
    ///
    public nonisolated(unsafe) let storedUserInfo: [AnyHashable : Any]?

    // MARK: - Initializers

    ///
    /// - Parameters:
    ///     - contentPolicy: The raw value of a case in `NSFileProviderContentPolicy`. This type erasure is necessary due to macOS 13 compatibility.
    ///
    public init(
        capabilities: NSFileProviderItemCapabilities,
        childItemCount: NSNumber?,
        contentModificationDate: Date?,
        contentPolicy: Int,
        contentType: UTType,
        creationDate: Date?,
        documentSize: NSNumber?,
        downloadingError: (any Error)?,
        filename: String,
        fileSystemFlags: NSFileProviderFileSystemFlags,
        isDownloaded: Bool,
        isDownloading: Bool,
        isMostRecentVersionDownloaded: Bool,
        isShared: Bool,
        isSharedByCurrentUser: Bool,
        isUploaded: Bool,
        isUploading: Bool,
        itemIdentifier: NSFileProviderItemIdentifier,
        itemVersion: NSFileProviderItemVersion,
        lastUsedDate: Date?,
        mostRecentEditorNameComponents: PersonNameComponents?,
        ocID: String?,
        ownerNameComponents: PersonNameComponents?,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        uploadingError: (any Error)?,
        userInfo: [AnyHashable : Any]?
    ) {
        self.capabilities = capabilities
        self.childItemCount = childItemCount
        self.contentModificationDate = contentModificationDate
        self.storedContentPolicy = contentPolicy
        self.contentType = contentType
        self.creationDate = creationDate
        self.documentSize = documentSize
        self.downloadingError = downloadingError
        self.filename = filename
        self.fileSystemFlags = fileSystemFlags
        self.isDownloaded = isDownloaded
        self.isDownloading = isDownloading
        self.isMostRecentVersionDownloaded = isMostRecentVersionDownloaded
        self.isShared = isShared
        self.isSharedByCurrentUser = isSharedByCurrentUser
        self.isUploaded = isUploaded
        self.isUploading = isUploading
        self.itemIdentifier = itemIdentifier
        self.itemVersion = itemVersion
        self.lastUsedDate = lastUsedDate
        self.mostRecentEditorNameComponents = mostRecentEditorNameComponents
        self.ocID = ocID
        self.ownerNameComponents = ownerNameComponents
        self.parentItemIdentifier = parentItemIdentifier
        self.uploadingError = uploadingError
        self.storedUserInfo = userInfo

        super.init()
    }
}
