//
//  Eviction.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 18/3/25.
//

import FileProvider
import Foundation
import OSLog

func evict(
    itemsWithIdentifiers identifiers: [NSFileProviderItemIdentifier],
    inDomain domain: NSFileProviderDomain
) async {
    Logger.eviction.debug("Starting eviction process…")
    guard let manager = NSFileProviderManager(for: domain) else {
        Logger.eviction.error(
            "Could not get manager for domain: \(domain.identifier.rawValue, privacy: .public)"
        )
        return;
    }
    do {
        for itemIdentifier in identifiers {
            Logger.eviction.error(
                "Evicting item: \(itemIdentifier.rawValue, privacy: .public)"
            )
            try await manager.evictItem(identifier: itemIdentifier)
        }
    } catch let error {
        Logger.eviction.error(
            "Error evicting item: \(error.localizedDescription, privacy: .public)"
        )
    }
}

func evict(
    itemsWithIdentifiers identifiers: [NSFileProviderItemIdentifier],
    inDomain domain: NSFileProviderDomain
) {
    let semaphore = DispatchSemaphore(value: 0)
    Task {
        await evict(itemsWithIdentifiers: identifiers, inDomain: domain)
        semaphore.signal()
    }
    semaphore.wait()
}
