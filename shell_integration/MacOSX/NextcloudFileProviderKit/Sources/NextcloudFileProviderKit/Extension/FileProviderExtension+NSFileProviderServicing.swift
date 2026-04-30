//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit
import OSLog

let AuthenticationTimeouts: [UInt64] = [ // Have progressively longer timeouts to not hammer the server
    3_000_000_000, 6_000_000_000, 30_000_000_000, 60_000_000_000, 120_000_000_000, 300_000_000_000
]

extension FileProviderExtension: NSFileProviderServicing {
    public func supportedServiceSources(for itemIdentifier: NSFileProviderItemIdentifier, completionHandler: @escaping ([NSFileProviderServiceSource]?, Error?) -> Void) -> Progress {
        logger.debug("Serving supported service sources...")
        let serviceSource = FPUIExtensionServiceSource(fpExtension: self)
        completionHandler([self, serviceSource], nil)
        let progress = Progress()

        // The framework's `cancellationHandler` is typed `@Sendable` but `completionHandler` is
        // not declared `@Sendable` by the protocol. We capture it through an unchecked Sendable
        // box so we can still report cancellation back to the framework.
        let completionBox = UncheckedSendable(value: completionHandler)
        progress.cancellationHandler = { [weak self] in
            let error = NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError)
            completionBox.value(nil, error)
            self?.logger.error("Cancellation handler of progress object for supported service sources call invoked!")
        }

        return progress
    }
}

private struct UncheckedSendable<Value>: @unchecked Sendable {
    let value: Value
}
