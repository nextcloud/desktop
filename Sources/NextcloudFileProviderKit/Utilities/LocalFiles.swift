//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import OSLog

fileprivate let lfuLogger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "", category: "localfileutils")

///
/// Resolve the path of the shared container for the app group of the file provider extension.
///
/// - Returns: Container URL for the extension's app group or `nil`, if it could not be found.
///
public func pathForAppGroupContainer() -> URL? {
    guard let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "NCFPKAppGroupIdentifier") as? String else {
        lfuLogger.error("Could not get app group container URL due to missing value for NCFPKAppGroupIdentifier key in Info.plist!")
        return nil
    }

    return FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: appGroupIdentifier)
}

public func pathForFileProviderTempFilesForDomain(_ domain: NSFileProviderDomain) throws -> URL? {
    guard let fpManager = NSFileProviderManager(for: domain) else {
        lfuLogger.error("Unable to get file provider manager for domain: \(domain.displayName, privacy: .public)")
        throw NSFileProviderError(.providerNotFound)
    }

    let fileProviderDataUrl = try fpManager.temporaryDirectoryURL()
    return fileProviderDataUrl.appendingPathComponent("TemporaryNextcloudFiles")
}

/// 
/// Determine whether the given filename is a lock file as created by certain applications like Microsoft Office or LibreOffice.
/// 
/// - Parameters:
///     - filename: The filename to check.
/// 
/// - Returns: `true` if the filename is a lock file, `false` otherwise.
/// 
public func isLockFileName(_ filename: String) -> Bool {
    // Microsoft Office lock files
    return filename.hasPrefix("~$") ||
        // LibreOffice lock files
        (filename.hasPrefix(".~lock.") && filename.hasSuffix("#"))
}

///
/// Parse the original file name contained in a lock filename.
///
/// Example for Microsoft Office: `MyDoc.docx` is extracted from `~$MyDoc.docx`.
/// Example for LibreOffice: `MyDoc.odt` is extracted from `.~lock.MyDoc.odt#`.
/// 
/// - Returns: Either the original file name parsed from the given lock file name or `nil`, if it is not a recognized lock file format.
///
public func originalFileName(fromLockFileName lockFilename: String) -> String? {
    if lockFilename.hasPrefix("~$") {
        // Remove the "~$" prefix
        let index = lockFilename.index(lockFilename.startIndex, offsetBy: 2)
        return String(lockFilename[index...])
    }

    if lockFilename.hasPrefix(".~lock.") && lockFilename.hasSuffix("#") {
        // Strip the prefix and suffix
        let start = lockFilename.index(lockFilename.startIndex, offsetBy: 7)
        let end = lockFilename.index(before: lockFilename.endIndex)
        return String(lockFilename[start..<end])
    }

    // Not a recognized lock file
    return nil
}
