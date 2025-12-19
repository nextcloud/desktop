//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

public extension FileManager {
    ///
    /// Return log directory specific to the file provider domain distinguished by the given identifier.
    ///
    /// If such directory does not exist yet, this attempts to create it implicitly.
    ///
    /// - Parameters:
    ///     - identifier: File provider domain identifier which is used to isolate log data for different file provider domains of the same extension.
    ///
    /// - Returns: A directory based on what the system returns for looking up standard directories. Likely in the sandbox containers of the file provider extension. Very unlikely to fail by returning `nil`.
    ///
    func fileProviderDomainLogDirectory(for identifier: NSFileProviderDomainIdentifier) -> URL? {
        guard let applicationGroupContainer = applicationGroupContainer() else {
            return nil
        }

        let logsDirectory = applicationGroupContainer
            .appendingPathComponent("File Provider Domains", isDirectory: true)
            .appendingPathComponent(identifier.rawValue, isDirectory: true)
            .appendingPathComponent("Logs", isDirectory: true)

        if fileExists(atPath: logsDirectory.path) == false {
            do {
                try createDirectory(at: logsDirectory, withIntermediateDirectories: true)
            } catch {
                return nil
            }
        }

        return logsDirectory
    }
}
