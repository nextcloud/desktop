//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import UniformTypeIdentifiers

///
/// The data model for file provider items of this extension as required by the file provider framework.
///
/// To enable thread-safety, this is a dedicated type and cannot be based on the data model entities.
///
public final class FileProviderItem: NSObject, Sendable {
    private let storedCapabilities: NSFileProviderItemCapabilities
    private let storedChildItemCount: NSNumber?
    private let storedContentModificationDate: Date?
    private let storedContentPolicy: Int
    private let storedContentType: UTType
    private let storedCreationDate: Date?
    private let storedDocumentSize: NSNumber?
    private let storedDownloadingError: (any Error)?
    private let storedFilename: String
    private let storedFileSystemFlags: NSFileProviderFileSystemFlags
    private let storedIsDownloaded: Bool
    private let storedIsDownloading: Bool
    private let storedIsMostRecentVersionDownloaded: Bool
    private let storedIsShared: Bool
    private let storedIsSharedByCurrentUser: Bool
    private let storedIsUploaded: Bool
    private let storedIsUploading: Bool
    private let storedItemIdentifier: NSFileProviderItemIdentifier

    ///
    /// `nonisolated(unsafe)` is necessary because some parts of the file provider framework have not been updated with full concurrency annotations yet.
    /// It is safe to use here, though, because it is a `let` defined in the initializer.
    ///
    private nonisolated(unsafe) let storedItemVersion: NSFileProviderItemVersion
    private let storedLastUsedDate: Date?
    private let storedMostRecentEditorNameComponents: PersonNameComponents?
    private let storedOwnerNameComponents: PersonNameComponents?
    private let storedParentItemIdentifier: NSFileProviderItemIdentifier
    private let storedUploadingError: (any Error)?

    ///
    /// `nonisolated(unsafe)` is necessary because some parts of the file provider framework have not been updated with full concurrency annotations yet.
    /// It is safe to use here, though, because it is a `let` defined in the initializer.
    ///
    private nonisolated(unsafe) let storedUserInfo: [AnyHashable : Any]?

    ///
    /// - Parameters:
    ///     - contentPolicy: The raw value of a case in `NSFileProviderContentPolicy`.
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
        ownerNameComponents: PersonNameComponents?,
        parentItemIdentifier: NSFileProviderItemIdentifier,
        uploadingError: (any Error)?,
        userInfo: [AnyHashable : Any]?
    ) {
        self.storedCapabilities = capabilities
        self.storedChildItemCount = childItemCount
        self.storedContentModificationDate = contentModificationDate
        self.storedContentPolicy = contentPolicy
        self.storedContentType = contentType
        self.storedCreationDate = creationDate
        self.storedDocumentSize = documentSize
        self.storedDownloadingError = downloadingError
        self.storedFilename = filename
        self.storedFileSystemFlags = fileSystemFlags
        self.storedIsDownloaded = isDownloaded
        self.storedIsDownloading = isDownloading
        self.storedIsMostRecentVersionDownloaded = isMostRecentVersionDownloaded
        self.storedIsShared = isShared
        self.storedIsSharedByCurrentUser = isSharedByCurrentUser
        self.storedIsUploaded = isUploaded
        self.storedIsUploading = isUploading
        self.storedItemIdentifier = itemIdentifier
        self.storedItemVersion = itemVersion
        self.storedLastUsedDate = lastUsedDate
        self.storedMostRecentEditorNameComponents = mostRecentEditorNameComponents
        self.storedOwnerNameComponents = ownerNameComponents
        self.storedParentItemIdentifier = parentItemIdentifier
        self.storedUploadingError = uploadingError
        self.storedUserInfo = userInfo

        super.init()
    }
}

extension FileProviderItem: NSFileProviderItem {
    public var capabilities: NSFileProviderItemCapabilities {
        storedCapabilities
    }

    public var childItemCount: NSNumber? {
        storedChildItemCount
    }

    public var contentModificationDate: Date? {
        storedContentModificationDate
    }

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

    public var contentType: UTType {
        storedContentType
    }

    public var creationDate: Date? {
        storedCreationDate
    }

    public var documentSize: NSNumber? {
        storedDocumentSize
    }

    public var downloadingError: (any Error)? {
        storedDownloadingError
    }

    public var filename: String {
        storedFilename
    }

    public var fileSystemFlags: NSFileProviderFileSystemFlags {
        storedFileSystemFlags
    }

    public var isDownloaded: Bool {
        storedIsDownloaded
    }

    public var isDownloading: Bool {
        storedIsDownloading
    }

    public var isMostRecentVersionDownloaded: Bool {
        storedIsMostRecentVersionDownloaded
    }

    public var isShared: Bool {
        storedIsShared
    }

    public var isSharedByCurrentUser: Bool {
        storedIsSharedByCurrentUser
    }

    public var isUploaded: Bool {
        storedIsUploaded
    }

    public var isUploading: Bool {
        storedIsUploading
    }

    public var itemIdentifier: NSFileProviderItemIdentifier {
        storedItemIdentifier
    }

    public var itemVersion: NSFileProviderItemVersion {
        storedItemVersion
    }

    public var lastUsedDate: Date? {
        storedLastUsedDate
    }

    public var mostRecentEditorNameComponents: PersonNameComponents? {
        storedMostRecentEditorNameComponents
    }

    public var ownerNameComponents: PersonNameComponents? {
        storedOwnerNameComponents
    }

    public var parentItemIdentifier: NSFileProviderItemIdentifier {
        storedParentItemIdentifier
    }

    public var uploadingError: (any Error)? {
        storedUploadingError
    }

    public var userInfo: [AnyHashable : Any]? {
        storedUserInfo
    }
}
