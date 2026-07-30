//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

extension FileProviderExtension: NSFileProviderServiceSource {
    func makeListenerEndpoint() throws -> NSXPCListenerEndpoint {
        logger.info("Making listener endpoint...")

        // Invalidate existing listener.
        listener.invalidate()

        // Set up a new listener.
        listener = NSXPCListener.anonymous()
        listener.delegate = self
        listener.resume()

        return listener.endpoint
    }
}
