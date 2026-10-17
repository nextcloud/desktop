//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks

public class MockEnumerator: NSObject, NSFileProviderEnumerator {
    let account: Account
    let dbManager: FilesDatabaseManager
    let remoteInterface: MockRemoteInterface
    public var enumeratorItems: [SendableItemMetadata] = []
    public var enumerateItemsHandler: ((any NSFileProviderEnumerationObserver, NSFileProviderPage) -> Void)?
    public private(set) var enumeratedPages: [NSFileProviderPage] = []
    public private(set) var invalidationCount = 0

    public init(
        account: Account, dbManager: FilesDatabaseManager, remoteInterface: MockRemoteInterface
    ) {
        self.account = account
        self.dbManager = dbManager
        self.remoteInterface = remoteInterface
    }

    public func enumerateItems(for observer: any NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        enumeratedPages.append(page)
        if let enumerateItemsHandler {
            enumerateItemsHandler(observer, page)
            return
        }
        let remoteSupportsTrash = remoteInterface.directMockCapabilities()?.files?.undelete ?? false
        var items: [Item] = []
        for item in enumeratorItems {
            guard let parentItemIdentifier = dbManager.parentItemIdentifierFromMetadata(item) else {
                print("Could not get parent item identifier for \(item)")
                continue
            }
            let item = Item(
                metadata: item,
                parentItemIdentifier: parentItemIdentifier,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                displayFileActions: false,
                remoteSupportsTrash: remoteSupportsTrash,
                log: FileProviderLogMock()
            )
            items.append(item)
        }
        observer.didEnumerate(items)
        observer.finishEnumerating(upTo: nil)
    }

    public func invalidate() {
        invalidationCount += 1
        print("MockEnumerator invalidated")
    }
}
