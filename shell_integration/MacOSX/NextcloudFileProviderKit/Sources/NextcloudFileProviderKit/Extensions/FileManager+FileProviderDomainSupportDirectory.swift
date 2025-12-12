//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

public extension FileManager {
    ///
    /// Return the application support directory specific to the file provider domain distinguished by the given identifier.
    ///
    /// If such directory does not exist yet, this attempts to create it implicitly.
    ///
    /// - Parameters:
    ///     - identifier: File provider domain identifier which is used to isolate application support data for different file provider domains of the same extension.
    ///
    /// - Returns: A directory based on what the system returns for looking up standard directories. Likely in the sandbox containers of the file provider extension. Very unlikely to fail by returning `nil`.
    ///
    func fileProviderDomainSupportDirectory(for identifier: NSFileProviderDomainIdentifier) -> URL? {
        guard let containerUrl = applicationGroupContainer() else {
            return nil
        }

        let supportDirectory = containerUrl
            .appendingPathComponent("File Provider Domains", isDirectory: true)
            .appendingPathComponent(identifier.rawValue, isDirectory: true)

        if fileExists(atPath: supportDirectory.path) == false {
            do {
                try createDirectory(at: supportDirectory, withIntermediateDirectories: true)
            } catch {
                return nil
            }
        }

        return supportDirectory
    }
}
