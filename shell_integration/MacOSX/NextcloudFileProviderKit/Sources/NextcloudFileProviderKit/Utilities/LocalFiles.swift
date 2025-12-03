//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import OSLog

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
    filename.hasPrefix("~$") ||
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
    var targetFileSuffix = lockFilename

    if lockFilename.hasPrefix("~$") {
        let index = lockFilename.index(lockFilename.startIndex, offsetBy: 2)
        targetFileSuffix = String(lockFilename[index...])
    }

    if lockFilename.hasPrefix(".~lock."), lockFilename.hasSuffix("#") {
        let start = lockFilename.index(lockFilename.startIndex, offsetBy: 7)
        let end = lockFilename.index(before: lockFilename.endIndex)
        targetFileSuffix = String(lockFilename[start ..< end])
    }

    if let sbRange = lockFilename.range(of: ".sb-") {
        targetFileSuffix = String(lockFilename[..<sbRange.lowerBound])
    }

    logger.debug("Target suffix is \"\(targetFileSuffix)\".")
    let itemsMatchingMetadata = dbManager.itemsMetadataByFileNameSuffix(suffix: targetFileSuffix)

    for file in itemsMatchingMetadata {
        let potentialOriginalFile = file.fileName

        if lockFilename == potentialOriginalFile {
            logger.debug("Lock filename \"\(lockFilename)\" is the same as filename found in database: \"\(potentialOriginalFile)\".")
            continue
        }

        logger.debug("Matched lock filename \"\(lockFilename)\" to original filename \"\(potentialOriginalFile)\".")
        return potentialOriginalFile
    }

    return nil
}
