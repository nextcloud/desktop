//
//  FileProviderExtension+CustomActions.swift
//  NextcloudIntegration
//
//  Created by Claudio Cambra on 14/5/25.
//

import FileProvider
import NextcloudFileProviderKit
import OSLog

extension FileProviderExtension: NSFileProviderCustomAction {
    func performAction(
        identifier actionIdentifier: NSFileProviderExtensionActionIdentifier,
        onItemsWithIdentifiers itemIdentifiers: [NSFileProviderItemIdentifier],
        completionHandler: @escaping ((any Error)?) -> Void
    ) -> Progress {
        let progress = Progress()
        return progress
    }
}
