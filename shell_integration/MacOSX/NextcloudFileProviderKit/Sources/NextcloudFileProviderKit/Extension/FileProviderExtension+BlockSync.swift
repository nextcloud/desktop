//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation

public extension FileProviderExtension {
    ///
    /// Whether synchronization with the server is currently blocked.
    ///
    /// A key which is absent, or set to something which is not a boolean, means synchronization is not blocked. A malformed value must never be a reason to stop synchronizing.
    ///
    var blockSync: Bool {
        config.blockSync ?? false
    }

    ///
    /// Watch for synchronization being unblocked, and let the system retry what it held back.
    ///
    /// The gates deliberately do not depend on this observation. It exists so that recovery is prompt rather than waiting for whatever the system would have done next: items the framework throttled because of the errors returned while blocked are released with `signalErrorResolved(_:completionHandler:)`, so that the work refused while blocked is asked for again. Should the notification ever be missed, the only consequence is that recovery waits for the next natural signal, and the client can never be left blocked.
    ///
    /// The working set is deliberately **not** signalled here. Doing so enumerates the server before the released uploads are retried, and an item which changed on both sides then has its base version refreshed by that enumeration — so the upload which follows no longer looks like a conflict and quietly overwrites the newer version on the server. Remote changes are found soon enough anyway, because the app is still polling or listening and signals the working set itself. Releasing the throttle is this observation's whole job.
    ///
    internal func observeBlockSync() {
        blockSyncObservation = UserDefaults.standard.observe(\.blockSync, options: [.new]) { [weak self] _, _ in
            guard let self, blockSync == false else {
                return
            }

            logger.info("Synchronization is no longer blocked, releasing throttled items.")

            manager?.signalErrorResolved(NSFileProviderError(.serverUnreachable)) { [weak self] error in
                guard let error else {
                    return
                }

                self?.logger.error("Could not report that synchronization is no longer blocked.", [.error: error])
            }
        }
    }
}
