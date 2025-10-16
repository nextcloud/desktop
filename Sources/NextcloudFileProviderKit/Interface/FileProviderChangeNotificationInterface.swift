//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation

public class FileProviderChangeNotificationInterface: ChangeNotificationInterface {
    let domain: NSFileProviderDomain
    let logger: FileProviderLogger

    required init(domain: NSFileProviderDomain, log: any FileProviderLogging) {
        self.domain = domain
        logger = FileProviderLogger(category: "FileProviderChangeNotificationInterface", log: log)
    }

    public func notifyChange() {
        Task { @MainActor in
            if let manager = NSFileProviderManager(for: domain) {
                do {
                    try await manager.signalEnumerator(for: .workingSet)
                } catch {
                    self.logger.error("Could not signal enumerator.", [.domain: self.domain.identifier, .error: error])
                }
            }
        }
    }
}
