//
//  Eviction.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 18/3/25.
//

import FileProvider
import Foundation

func evict(
    itemsWithIdentifiers identifiers: [NSFileProviderItemIdentifier],
    inDomain domain: NSFileProviderDomain
) async {
    guard let manager = NSFileProviderManager(for: domain) else {
        return;
    }
    do {
        for itemIdentifier in identifiers {
            try await manager.evictItem(identifier: itemIdentifier)
        }
    } catch let error {
        
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
