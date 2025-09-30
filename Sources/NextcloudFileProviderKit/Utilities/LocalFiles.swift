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
/// - Example for Microsoft Office: `MyDoc.docx` is extracted from `~$MyDoc.docx`.
/// - Example for LibreOffice: `MyDoc.odt` is extracted from `.~lock.MyDoc.odt#`.
/// - Filename with less than 8 characters like `Test.docx` will result in a lock file named `~$Test.docx`.
/// - Filename with more than 8 characters like `Document.docx` will result in a lock file named `~$cument.docx`.
/// - Filename sandbox-style temporary naming like `Welcome123456.doc.sb-d215eb53-IBAwfU`.
///
/// - Returns: Either the original file name parsed from the given lock file name or `nil`, if it is not a recognized lock file format.
///
public func originalFileName(fromLockFileName lockFilename: String, dbManager: FilesDatabaseManager) -> String? {
    let logger = FileProviderLogger(category: "LocalFiles", log: dbManager.logger.log)
    logger.debug("Called originalFileName with lock filename: \(lockFilename)")

    var targetFileSuffix = lockFilename
    if lockFilename.hasPrefix("~$") {
        let index = lockFilename.index(lockFilename.startIndex, offsetBy: 2)
        targetFileSuffix = String(lockFilename[index...])
    }

    if lockFilename.hasPrefix(".~lock.") && lockFilename.hasSuffix("#") {
        let start = lockFilename.index(lockFilename.startIndex, offsetBy: 7)
        let end = lockFilename.index(before: lockFilename.endIndex)
        targetFileSuffix = String(lockFilename[start..<end])
    }
    
    if let sbRange = lockFilename.range(of: ".sb-") {
        targetFileSuffix = String(lockFilename[..<sbRange.lowerBound])
    }

    logger.debug("Target suffix is: \(targetFileSuffix)")

    let itemsMatchingMetadata = dbManager.itemsMetadataByFileNameSuffix(suffix: targetFileSuffix)
    for file in itemsMatchingMetadata {
        let potentialOriginalFile = file.fileName
        
        if lockFilename == potentialOriginalFile {
            logger.debug("Lock filename \(lockFilename) is the same as filename found in db \(potentialOriginalFile)")
            continue;
        }

        logger.debug("Matched lock filename \(lockFilename) to original filename \(potentialOriginalFile)")
        return potentialOriginalFile
    }
    
    return nil
}
