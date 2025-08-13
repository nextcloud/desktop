//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation

public extension FileManager {
    ///
    /// Return the sandboxed application support directory specific to the file provider domain distinguished by the given identifier.
    ///
    /// If such directory does not exist yet, this attempts to create it implicitly.
    ///
    /// > Legacy Support: In the past, a subdirectory in the application group container was used for everything.
    /// This caused crashes due to violations of sandbox restrictions.
    /// If already existent, the legacy location will be used.
    /// Otherwise the data will be stored in a new location.
    ///
    /// - Parameters:
    ///     - identifier: File provider domain identifier which is used to isolate application support data for different file provider domains of the same extension.
    ///
    /// - Returns: A directory based on what the system returns for looking up standard directories. Likely in the sandbox containers of the file provider extension. Very unlikely to fail by returning `nil`.
    ///
    func fileProviderDomainSupportDirectory(for identifier: NSFileProviderDomainIdentifier) -> URL? {
        // Legacy directory support.
        if let containerUrl = pathForAppGroupContainer() {
            let legacyLocation = containerUrl.appendingPathComponent("FileProviderExt")

            if FileManager.default.fileExists(atPath: legacyLocation.path) {
                return legacyLocation
            }
        }

        // Designated file provider domain directories.
        guard let applicationSupportDirectory = try? url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true) else {
            return nil
        }

        let domainsSupportDirectory = applicationSupportDirectory.appendingPathComponent("File Provider Domains")
        let fileProviderDomainSupportDirectory = domainsSupportDirectory.appendingPathComponent(identifier.rawValue)

        if fileExists(atPath: fileProviderDomainSupportDirectory.path) == false {
            do {
                try createDirectory(at: fileProviderDomainSupportDirectory, withIntermediateDirectories: true)
            } catch {
                return nil
            }
        }

        return fileProviderDomainSupportDirectory
    }
}
