//
//  LockViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 30/7/24.
//

import AppKit
import FileProvider
import NextcloudKit
import OSLog
import QuickLookThumbnailing

class LockViewController: NSViewController {
    let itemIdentifiers: [NSFileProviderItemIdentifier]
    let locking: Bool

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier], locking: Bool) {
        self.itemIdentifiers = itemIdentifiers
        self.locking = locking
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
