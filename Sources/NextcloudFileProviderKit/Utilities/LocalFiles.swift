/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
import Foundation
import OSLog

fileprivate let lfuLogger = Logger(subsystem: Logger.subsystem, category: "localfileutils")

///
/// Resolve the path of the shared container for the app group of the file provider extension.
///
/// - Returns: Container URL for the extension's app group.
///
public func pathForAppGroupContainer() -> URL? {
    guard let appGroupIdentifier = Bundle.main.object(
        forInfoDictionaryKey: "NCFPKAppGroupIdentifier"
    ) as? String else {
        lfuLogger.critical(
            "Could not get app group container url as Info.plist missing NCFPKAppGroupIdentifier!"
        )
        return nil
    }

    return FileManager.default.containerURL(
        forSecurityApplicationGroupIdentifier: appGroupIdentifier)
}

///
/// Resolve the path where the file provider extension store its data.
///
/// - Returns: The root location in which the extension can store its specific data.
///
public func pathForFileProviderExtData() -> URL? {
    let containerUrl = pathForAppGroupContainer()
    return containerUrl?.appendingPathComponent("FileProviderExt")
}

public func pathForFileProviderTempFilesForDomain(_ domain: NSFileProviderDomain) throws -> URL? {
    guard let fpManager = NSFileProviderManager(for: domain) else {
        lfuLogger.error(
            "Unable to get file provider manager for domain: \(domain.displayName, privacy: .public)"
        )
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
