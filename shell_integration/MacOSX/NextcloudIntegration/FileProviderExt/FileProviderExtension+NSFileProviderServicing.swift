//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NCDesktopClientSocketKit
import NextcloudKit
import NextcloudFileProviderKit
import OSLog

let AuthenticationTimeouts: [UInt64] = [ // Have progressively longer timeouts to not hammer the server
    3_000_000_000, 6_000_000_000, 30_000_000_000, 60_000_000_000, 120_000_000_000, 300_000_000_000
]

extension FileProviderExtension: NSFileProviderServicing {
    func supportedServiceSources(for itemIdentifier: NSFileProviderItemIdentifier, completionHandler: @escaping ([NSFileProviderServiceSource]?, Error?) -> Void) -> Progress {
        logger.debug("Serving supported service sources...")
        let serviceSource = FPUIExtensionServiceSource(fpExtension: self)
        completionHandler([self, serviceSource], nil)
        let progress = Progress()

        progress.cancellationHandler = {
            let error = NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError)
            completionHandler(nil, error)
            self.logger.error("Cancellation handler of progress object for supported service sources call invoked!")
        }

        return progress
    }
}
