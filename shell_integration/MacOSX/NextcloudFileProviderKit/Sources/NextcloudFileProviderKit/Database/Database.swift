//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import CoreData
import FileProvider
import os
import UniformTypeIdentifiers

///
/// Default implementation of ``DatabaseManaging``.
///
/// It abstracts a CoreData stack and has a concurrency-safe interface with only `Sendable` types.
///
/// The special trash container is maintained internally for consistency regardless of server support.
/// The root container always is expected to exist.
///
/// It features extensive logging based on ``FileProviderLogging`` and records performance data through native signposts which can be inspected in Instruments.
///
actor Database {
    let logger: FileProviderLogger
    var mappedIdentifiers: [NSFileProviderItemIdentifier: NSFileProviderItemIdentifier]
    let personNameComponentsFormatter: PersonNameComponentsFormatter
    let signposter: OSSignposter

    ///
    /// The user name of the Nextcloud user this database synchronizes with.
    ///
    /// This is required information to check for ownership in locks, as an example.
    ///
    let user: String

    lazy var persistentContainer: NSPersistentContainer = {
        let container = NSPersistentContainer(name: "Database")

        container.loadPersistentStores { _, error in
            if let error {
                self.logger.fault("Failed to load persistent stores: \(error.localizedDescription)")
            }
        }

        return container
    }()

    // MARK: - Private

    private func item(by predicate: NSPredicate) throws -> FileProviderItem {
        let context = persistentContainer.newBackgroundContext()
        let fetchRequest = NSFetchRequest<NSManagedObject>(entityName: DatabaseItem.className())
        fetchRequest.predicate = predicate
        fetchRequest.fetchLimit = 1

        let results = try context.fetch(fetchRequest)

        guard let result = results.first as? DatabaseItem else {
            throw DatabaseError.databaseItemNotFound
        }

        let childItemCountFetchRequest = NSFetchRequest<NSManagedObject>(entityName: DatabaseItem.className())
        childItemCountFetchRequest.predicate = predicate
        let childItemCount = try context.count(for: childItemCountFetchRequest)

        return try map(result, childItemCount: childItemCount)
    }

    ///
    /// Convert a database item to a file provider item.
    ///
    /// This is required to not expose the managed objects which are not safe to pass across concurrency contexts.
    ///
    /// - Throws: If a property of the given managed object as no value when one is expected.
    ///
    private func map(_ databaseItem: DatabaseItem, childItemCount: Int?) throws -> FileProviderItem {
        var capabilities = NSFileProviderItemCapabilities()

        if databaseItem.allowsAddingSubItems {
            capabilities.insert(.allowsAddingSubItems)
        }

        if databaseItem.allowsContentEnumerating {
            capabilities.insert(.allowsContentEnumerating)
        }

        if databaseItem.allowsDeleting {
            capabilities.insert(.allowsDeleting)
        }

        if databaseItem.allowsEvicting {
            capabilities.insert(.allowsEvicting)
        }

        if databaseItem.allowsExcludingFromSync {
            capabilities.insert(.allowsExcludingFromSync)
        }

        if databaseItem.allowsReading {
            capabilities.insert(.allowsReading)
        }

        if databaseItem.allowsRenaming {
            capabilities.insert(.allowsRenaming)
        }

        if databaseItem.allowsReparenting {
            capabilities.insert(.allowsReparenting)
        }

        if databaseItem.allowsTrashing {
            capabilities.insert(.allowsTrashing)
        }

        if databaseItem.allowsWriting {
            capabilities.insert(.allowsWriting)
        }

        let childItemCount: NSNumber? = if let childItemCount {
            NSNumber(value: childItemCount)
        } else {
            nil
        }

        guard let contentTypeIdentifier = databaseItem.contentType, let contentType = UTType(contentTypeIdentifier) else {
            throw DatabaseError.missingValue
        }

        let downloadingError: DatabaseError? = if let localizedDescription = databaseItem.downloadingError {
            DatabaseError.persisted(localizedDescription: localizedDescription)
        } else {
            nil
        }

        guard let filename = databaseItem.filename else {
            throw DatabaseError.missingValue
        }

        var fileSystemFlags: NSFileProviderFileSystemFlags = [
            .userReadable,
            .userWritable
        ]

        if databaseItem.isLockFileOfLocalOrigin {
            fileSystemFlags = [
                .hidden,
                .userReadable,
                .userWritable
            ]
        }

        if let lock = databaseItem.lock {
            // Extract lock properties to avoid data race with non-Sendable managed object
            let lockType = lock.type
            let lockOwner = lock.owner
            let lockTimeOut = lock.timeOut
            
            if lockType != 0 /* manual user lock */ || lockOwner != user, lockTimeOut ?? Date() > Date() {
                fileSystemFlags = [
                    .userReadable
                ]
            }
        }

        guard let rawItemIdentifier = databaseItem.itemIdentifier else {
            throw DatabaseError.missingValue
        }

        let itemIdentifier = NSFileProviderItemIdentifier(rawItemIdentifier.uuidString)
        let itemVersion = NSFileProviderItemVersion(entityTag: databaseItem.itemVersion ?? "")

        let mostRecentEditorNameComponents: PersonNameComponents? = if let mostRecentEditorNameComponents = databaseItem.mostRecentEditorNameComponents {
            personNameComponentsFormatter.personNameComponents(from: mostRecentEditorNameComponents)
        } else {
            nil
        }

        guard let ocID = databaseItem.ocID else {
            throw DatabaseError.missingValue
        }

        let ownerNameComponents: PersonNameComponents? = if let ownerNameComponents = databaseItem.ownerNameComponents {
            personNameComponentsFormatter.personNameComponents(from: ownerNameComponents)
        } else {
            nil
        }

        let parentItemIdentifier = if let parentItemIdentifier = databaseItem.parentItemIdentifier {
            NSFileProviderItemIdentifier(parentItemIdentifier.uuidString)
        } else {
            itemIdentifier
        }

        let uploadingError: DatabaseError? = if let localizedDescription = databaseItem.uploadingError {
            DatabaseError.persisted(localizedDescription: localizedDescription)
        } else {
            nil
        }

        var userInfo = [AnyHashable: Any]()

        if let lock = databaseItem.lock {
            userInfo["locked"] = true
        }

        if #available(macOS 13.0, *) {
            userInfo["displayKeepDownloaded"] = databaseItem.contentPolicy != NSFileProviderContentPolicy.downloadEagerlyAndKeepDownloaded.rawValue
            userInfo["displayAllowAutoEvicting"] = databaseItem.contentPolicy != NSFileProviderContentPolicy.inherited.rawValue
            userInfo["displayEvict"] = databaseItem.isDownloaded && databaseItem.contentPolicy == NSFileProviderContentPolicy.downloadEagerlyAndKeepDownloaded.rawValue
        }

        // https://docs.nextcloud.com/server/latest/developer_manual/client_apis/WebDAV/basic.html
        if databaseItem.allowsSharing && itemIdentifier != .rootContainer && itemIdentifier != .trashContainer {
            userInfo["displayShare"] = true
        }

        return FileProviderItem(
            capabilities: capabilities,
            childItemCount: childItemCount,
            contentModificationDate: databaseItem.contentModificationDate,
            contentPolicy: Int(databaseItem.contentPolicy),
            contentType: contentType,
            creationDate: databaseItem.creationDate,
            documentSize: NSNumber(value: databaseItem.documentSize),
            downloadingError: downloadingError,
            filename: filename,
            fileSystemFlags: fileSystemFlags,
            isDownloaded: databaseItem.isDownloaded,
            isDownloading: databaseItem.isDownloading,
            isMostRecentVersionDownloaded: databaseItem.isMostRecentVersionDownloaded,
            isShared: databaseItem.isShared,
            isSharedByCurrentUser: databaseItem.isSharedByCurrentUser,
            isUploaded: databaseItem.isUploaded,
            isUploading: databaseItem.isUploading,
            itemIdentifier: itemIdentifier,
            itemVersion: itemVersion,
            lastUsedDate: databaseItem.lastUsedDate,
            mostRecentEditorNameComponents: mostRecentEditorNameComponents,
            ocID: ocID,
            ownerNameComponents: ownerNameComponents,
            parentItemIdentifier: parentItemIdentifier,
            uploadingError: uploadingError,
            userInfo: userInfo
        )
    }

    // MARK: - Public

    public init(log: any FileProviderLogging, user: String) {
        logger = FileProviderLogger(category: "Database", log: log)
        mappedIdentifiers = [:]
        personNameComponentsFormatter = PersonNameComponentsFormatter()
        signposter = OSSignposter(subsystem: Bundle.main.bundleIdentifier!, category: "Database")
        self.user = user
    }

    ///
    /// Insert a file provider item into the store.
    ///
    public func insert(_ item: FileProviderItem) throws {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("insertItem", id: signpostID)

        defer {
            signposter.endInterval("insertItem", interval)
        }

        guard let documentSize = item.documentSize?.int64Value else {
            throw DatabaseError.missingValue
        }

        guard let itemIdentifier = UUID(uuidString: item.itemIdentifier.rawValue) else {
            throw DatabaseError.invalidArgument
        }

        guard let parentItemIdentifier = UUID(uuidString: item.parentItemIdentifier.rawValue) else {
            throw DatabaseError.invalidArgument
        }

        let context = persistentContainer.newBackgroundContext()
        let managedObject = NSEntityDescription.insertNewObject(forEntityName: DatabaseItem.className(), into: context)

        guard let databaseItem = managedObject as? DatabaseItem else {
            throw DatabaseError.failedDowncast
        }

        databaseItem.allowsAddingSubItems = item.capabilities.contains(.allowsAddingSubItems)
        databaseItem.allowsContentEnumerating = item.capabilities.contains(.allowsContentEnumerating)
        databaseItem.allowsDeleting = item.capabilities.contains(.allowsDeleting)
        databaseItem.allowsEvicting = item.capabilities.contains(.allowsEvicting)
        databaseItem.allowsExcludingFromSync = item.capabilities.contains(.allowsExcludingFromSync)
        databaseItem.allowsReading = item.capabilities.contains(.allowsReading)
        databaseItem.allowsRenaming = item.capabilities.contains(.allowsRenaming)
        databaseItem.allowsReparenting = item.capabilities.contains(.allowsReparenting)
        databaseItem.allowsTrashing = item.capabilities.contains(.allowsTrashing)
        databaseItem.allowsWriting = item.capabilities.contains(.allowsWriting)
        databaseItem.contentModificationDate = item.contentModificationDate

        if #available(macOS 13.0, *) {
            databaseItem.contentPolicy = Int64(item.contentPolicy.rawValue)
        }

        databaseItem.contentType = item.contentType.identifier
        databaseItem.creationDate = item.creationDate
        databaseItem.documentSize = documentSize
        databaseItem.downloadingError = item.downloadingError?.localizedDescription
        databaseItem.filename = item.filename
        databaseItem.isDownloaded = item.isDownloaded
        databaseItem.isDownloading = item.isDownloading
        databaseItem.isMostRecentVersionDownloaded = item.isMostRecentVersionDownloaded
        databaseItem.isShared = item.isShared
        databaseItem.isSharedByCurrentUser = item.isSharedByCurrentUser
        databaseItem.isUploaded = item.isUploaded
        databaseItem.isUploading = item.isUploading
        databaseItem.itemIdentifier = itemIdentifier
        databaseItem.itemVersion = item.itemVersion.entityTag
        databaseItem.lastUsedDate = item.lastUsedDate
        databaseItem.mostRecentEditorNameComponents = item.mostRecentEditorNameComponents?.formatted()
        databaseItem.ocID = item.ocID
        databaseItem.ownerNameComponents = item.ownerNameComponents?.formatted()
        databaseItem.parentItemIdentifier = parentItemIdentifier
        databaseItem.uploadingError = item.uploadingError?.localizedDescription

        try context.save()
    }

    ///
    /// Insert a mapped identifier into the store.
    ///
    /// - Parameters:
    ///     - source: The identifier which is used to look up a mapped identifier.
    ///     - target: The identifier the source is supposed to be resolved to.
    ///
    public func insert(map source: NSFileProviderItemIdentifier, to target: NSFileProviderItemIdentifier) throws {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("insertMappedIdentifier", id: signpostID)

        defer {
            signposter.endInterval("insertMappedIdentifier", interval)
        }

        guard let targetUUID = UUID(uuidString: target.rawValue) else {
            throw DatabaseError.invalidArgument
        }

        let context = persistentContainer.newBackgroundContext()
        let managedObject = NSEntityDescription.insertNewObject(forEntityName: MappedIdentifier.className(), into: context)

        guard let mappedIdentifier = managedObject as? MappedIdentifier else {
            throw DatabaseError.failedDowncast
        }

        mappedIdentifier.itemIdentifier = source.rawValue
        mappedIdentifier.uuid = targetUUID

        try context.save()
        mappedIdentifiers[source] = target // Update the in-memory cache
    }

    public func item(by identifier: NSFileProviderItemIdentifier) throws -> FileProviderItem {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("itemByIdentifier", id: signpostID)

        defer {
            signposter.endInterval("itemByIdentifier", interval)
        }

        let identifier = map(identifier)
        let predicate = NSPredicate(format: "itemIdentifier == %@", identifier.rawValue)

        return try item(by: predicate)
    }

    public func item(byRemoteIdentifier identifier: String) throws -> FileProviderItem {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("itemByRemoteIdentifier", id: signpostID)

        defer {
            signposter.endInterval("itemByRemoteIdentifier", interval)
        }

        let predicate = NSPredicate(format: "ocID == %@", identifier)

        return try item(by: predicate)
    }

    ///
    /// Map a file provider item identifier to a UUID.
    ///
    /// This is required to have a uniform handling and property type, even for containers defined by the framework like root or trash.
    ///
    /// To reduce load on the CoreData store, this caches the results in memory in a dictionary.
    /// This is an acceptable memory overhead because usually only two items are expected.
    ///
    /// - Parameters:
    ///     - identifier: The file provider item identifier to look up an alias for.
    ///
    /// - Returns: Either the resolved identifier or the original argument as a fallback in case of error or failure to find.
    ///
    public func map(_ identifier: NSFileProviderItemIdentifier) -> NSFileProviderItemIdentifier {
        guard identifier == .rootContainer || identifier == .trashContainer else {
            return identifier
        }

        if let cachedIdentifier = mappedIdentifiers[identifier] {
            return cachedIdentifier
        }

        let context = persistentContainer.newBackgroundContext()
        let fetchRequest = NSFetchRequest<NSManagedObject>(entityName: MappedIdentifier.className())
        fetchRequest.predicate = NSPredicate(format: "itemIdentifier == %@", identifier.rawValue)
        fetchRequest.fetchLimit = 1

        do {
            let results = try context.fetch(fetchRequest)

            if results.isEmpty && identifier == .rootContainer {
                let uuid = UUID()
                let itemIdentifier = NSFileProviderItemIdentifier(uuid.uuidString)

                let contentPolicy = if #available(macOS 13.0, *) {
                    NSFileProviderContentPolicy.downloadLazily.rawValue
                } else {
                    1 // Equals the raw value of NSFileProviderContentPolicy.downloadLazily
                }

                logger.debug("Adding mapped identifier for root container.", [.item: uuid])

                try insert(FileProviderItem(
                    capabilities: [.allowsContentEnumerating, .allowsReading],
                    childItemCount: NSNumber(value: 0),
                    contentModificationDate: nil,
                    contentPolicy: contentPolicy,
                    contentType: .folder,
                    creationDate: Date(),
                    documentSize: NSNumber(value: 0),
                    downloadingError: nil,
                    filename: NSFileProviderItemIdentifier.rootContainer.rawValue,
                    fileSystemFlags: [.userReadable, .userWritable],
                    isDownloaded: true,
                    isDownloading: false,
                    isMostRecentVersionDownloaded: true,
                    isShared: false,
                    isSharedByCurrentUser: false,
                    isUploaded: true,
                    isUploading: false,
                    itemIdentifier: .rootContainer,
                    itemVersion: NSFileProviderItemVersion(),
                    lastUsedDate: nil,
                    mostRecentEditorNameComponents: nil,
                    ocID: nil,
                    ownerNameComponents: nil,
                    parentItemIdentifier: .rootContainer,
                    uploadingError: nil,
                    userInfo: nil
                ))
            }

            if results.isEmpty && identifier == .trashContainer {
                let uuid = UUID()
                let itemIdentifier = NSFileProviderItemIdentifier(uuid.uuidString)

                let contentPolicy = if #available(macOS 13.0, *) {
                    NSFileProviderContentPolicy.downloadLazily.rawValue
                } else {
                    1 // Equals the raw value of NSFileProviderContentPolicy.downloadLazily
                }

                logger.debug("Adding mapped identifier for trash container.", [.item: uuid])

                try insert(FileProviderItem(
                    capabilities: [.allowsContentEnumerating, .allowsReading],
                    childItemCount: NSNumber(value: 0),
                    contentModificationDate: nil,
                    contentPolicy: contentPolicy,
                    contentType: .folder,
                    creationDate: Date(),
                    documentSize: NSNumber(value: 0),
                    downloadingError: nil,
                    filename: NSFileProviderItemIdentifier.trashContainer.rawValue,
                    fileSystemFlags: [.userReadable, .userWritable],
                    isDownloaded: true,
                    isDownloading: false,
                    isMostRecentVersionDownloaded: true,
                    isShared: false,
                    isSharedByCurrentUser: false,
                    isUploaded: true,
                    isUploading: false,
                    itemIdentifier: .trashContainer,
                    itemVersion: NSFileProviderItemVersion(),
                    lastUsedDate: nil,
                    mostRecentEditorNameComponents: nil,
                    ocID: nil,
                    ownerNameComponents: nil,
                    parentItemIdentifier: .trashContainer,
                    uploadingError: nil,
                    userInfo: nil
                ))
            }

            guard let result = results.first, let uuid = result.value(forKey: "uuid") as? String else {
                logger.error("Failed to find mapped identifier!", [.item: identifier])
                return identifier
            }

            let finalIdentifier = NSFileProviderItemIdentifier(uuid)
            mappedIdentifiers[identifier] = finalIdentifier

            return finalIdentifier
        } catch {
            logger.fault("Failed to fetch mapped identifier: \(error.localizedDescription)")
        }

        return identifier
    }

    ///
    /// Getter for the root container item for this file provider domain.
    ///
    public func rootContainer() throws -> FileProviderItem {
        try item(by: .rootContainer)
    }

    ///
    /// Getter for the trash container item for this file provider domain.
    ///
    public func trashContainer() throws -> FileProviderItem {
        try item(by: .trashContainer)
    }
}
