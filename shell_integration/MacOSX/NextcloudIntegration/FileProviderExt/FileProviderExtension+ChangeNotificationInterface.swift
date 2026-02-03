//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudFileProviderKit

extension FileProviderExtension: ChangeNotificationInterface {
    func notifyChange() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            logger.error("Could not get file provider manager for domain \(self.domain.displayName), cannot notify changes")
            return
        }

        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                self.logger.error("Error signalling enumerator for working set, received error: \(error!.localizedDescription)")
            }
        }
    }
}
