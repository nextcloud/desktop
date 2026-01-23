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
    let signposter: OSSignposter

    lazy var persistentContainer: NSPersistentContainer = {
        let container = NSPersistentContainer(name: "Database")

        container.loadPersistentStores { _, error in
            if let error {
                self.logger.fault("Failed to load persistent stores: \(error.localizedDescription)")
            }
        }

        return container
    }()

    public init(log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "Database", log: log)
        mappedIdentifiers = [:]
        signposter = OSSignposter(subsystem: Bundle.main.bundleIdentifier!, category: "Database")

        // TODO: Seed database with items for root and trash container
        // TODO: Seed database with mapped identifiers
    }

    ///
    /// Insert a file provider item into the store.
    ///
    public func insert(_ item: FileProviderItem) throws {
        <#code#>
    }

    ///
    /// Insert a mapped identifier into the store.
    ///
    public func insert(map source: NSFileProviderItemIdentifier, to target: NSFileProviderItemIdentifier) throws {
        <#code#>
    }

    public func item(by identifier: NSFileProviderItemIdentifier) throws -> FileProviderItem {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("itemByIdentifier", id: signpostID)

        defer {
            signposter.endInterval("itemByIdentifier", interval)
        }

        let identifier = map(identifier)
        let context = persistentContainer.viewContext
        let fetchRequest = NSFetchRequest<NSManagedObject>(entityName: "DatabaseItem")
        fetchRequest.predicate = NSPredicate(format: "itemIdentifier == %@", identifier.rawValue)
        fetchRequest.fetchLimit = 1

        let results = try context.fetch(fetchRequest)

        guard let result = results.first else {
            throw DatabaseError.databaseItemNotFound(identifier)
        }

        return FileProviderItem(capabilities: <#T##NSFileProviderItemCapabilities#>, childItemCount: <#T##NSNumber?#>, contentModificationDate: <#T##Date?#>, contentPolicy: <#T##Int#>, contentType: <#T##UTType#>, creationDate: <#T##Date?#>, documentSize: <#T##NSNumber?#>, downloadingError: <#T##(any Error)?#>, filename: <#T##String#>, fileSystemFlags: <#T##NSFileProviderFileSystemFlags#>, isDownloaded: <#T##Bool#>, isDownloading: <#T##Bool#>, isMostRecentVersionDownloaded: <#T##Bool#>, isShared: <#T##Bool#>, isSharedByCurrentUser: <#T##Bool#>, isUploaded: <#T##Bool#>, isUploading: <#T##Bool#>, itemIdentifier: <#T##NSFileProviderItemIdentifier#>, itemVersion: <#T##NSFileProviderItemVersion#>, lastUsedDate: <#T##Date?#>, mostRecentEditorNameComponents: <#T##PersonNameComponents?#>, ownerNameComponents: <#T##PersonNameComponents?#>, parentItemIdentifier: <#T##NSFileProviderItemIdentifier#>, uploadingError: <#T##(any Error)?#>, userInfo: <#T##[AnyHashable : Any]?#>)
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

        let context = persistentContainer.viewContext
        let fetchRequest = NSFetchRequest<NSManagedObject>(entityName: "MappedIdentifier")
        fetchRequest.predicate = NSPredicate(format: "itemIdentifier == %@", identifier.rawValue)
        fetchRequest.fetchLimit = 1

        do {
            let results = try context.fetch(fetchRequest)

            guard let result = results.first, let uuid = result.value(forKey: "uuid") as? String else {
                logger.error("Failed to find mapped identifier!", [.item: identifier])
                return identifier
            }

            let finalIdentifier = NSFileProviderItemIdentifier(uuid)
            mappedIdentifiers[identifier] = finalIdentifier

            return finalIdentifier
        } catch {
            logger.fault("Failed to fetch MappedIdentifier: \(error.localizedDescription)")
        }

        return identifier
    }

    ///
    /// Getter for the root container item for this file provider domain.
    ///
    public func rootContainer() throws -> FileProviderItem {
        let item = try item(by: .rootContainer)
    }

    ///
    /// Getter for the trash container item for this file provider domain.
    ///
    public func trashContainer() throws -> FileProviderItem {
        let item = try item(by: .trashContainer)
    }
}
