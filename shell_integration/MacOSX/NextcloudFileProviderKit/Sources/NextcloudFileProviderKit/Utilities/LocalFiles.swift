//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import OSLog

/// Lock file extensions created by Adobe applications, mapped to the document extension(s)
/// the lock file may guard.
///
/// Unlike Microsoft Office (`~$…`) and LibreOffice (`.~lock.…#`) lock files, Adobe lock file
/// names do not encode the guarded document's own extension — only its base name — so the
/// document has to be located among the lock file's siblings. These extensions are exclusively
/// used for transient lock files (no legitimate user document uses them), which makes matching
/// by extension safe from false positives.
///
/// - `idlk`: InDesign documents (`indd`) and InCopy stories (`icml`).
/// - `prlock`: Premiere Pro projects (`prproj`).
let adobeLockFileDocumentExtensions: [String: [String]] = [
    "idlk": ["indd", "icml"],
    "prlock": ["prproj"]
]

/// Lock file extensions created by AutoCAD.
///
/// AutoCAD creates `.dwl` (plain text) and `.dwl2` (XML) lock files when a `.dwg` drawing is
/// opened. Unlike Adobe lock files, the lock file shares the exact same base name as the
/// document — only the extension differs — so the guarded document is resolved by simply
/// replacing the lock extension with `.dwg`. Both lock files are deleted when the drawing
/// is closed.
let autoCADLockFileExtensions: Set<String> = ["dwl", "dwl2"]

/// The document extension guarded by AutoCAD lock files.
let autoCADDocumentExtension = "dwg"

///
/// Determine whether the given filename is a lock file as created by Adobe applications like InDesign or Premiere Pro.
///
/// - Parameters:
///     - filename: The filename to check.
///
/// - Returns: `true` if the filename is an Adobe lock file, `false` otherwise.
///
public func isAdobeLockFileName(_ filename: String) -> Bool {
    adobeLockFileDocumentExtensions.keys.contains((filename as NSString).pathExtension.lowercased())
}

///
/// Determine whether the given filename is a lock file as created by AutoCAD.
///
/// - Parameters:
///     - filename: The filename to check.
///
/// - Returns: `true` if the filename is an AutoCAD lock file, `false` otherwise.
///
public func isAutoCADLockFileName(_ filename: String) -> Bool {
    autoCADLockFileExtensions.contains((filename as NSString).pathExtension.lowercased())
}

///
/// Determine whether the given filename is a lock file as created by certain applications like Microsoft Office, LibreOffice, Adobe or AutoCAD.
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
        (filename.hasPrefix(".~lock.") && filename.hasSuffix("#")) ||
        // Adobe lock files
        isAdobeLockFileName(filename) ||
        // AutoCAD lock files
        isAutoCADLockFileName(filename)
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

///
/// Extract the document base name embedded in an Adobe lock file name.
///
/// - Example for InDesign / InCopy: `Test` is extracted from `~Test~0kjyv(.idlk`.
/// - Example for Premiere Pro: `Test` is extracted from `Test.prlock`.
///
/// Adobe lock file names drop the guarded document's own extension, so only the base name can
/// be recovered here. The matching document is resolved separately via ``adobeLockFileTargetName(lockFilename:parentServerUrl:dbManager:)``.
///
/// - Returns: The document base name, or `nil` if it cannot be determined.
///
func adobeLockFileDocumentBaseName(_ lockFilename: String) -> String? {
    let ext = (lockFilename as NSString).pathExtension.lowercased()
    var stem = (lockFilename as NSString).deletingPathExtension

    switch ext {
        case "idlk":
            // InDesign / InCopy: `~{base name}~{random token}(.idlk`.
            if stem.hasPrefix("~") {
                stem.removeFirst()
            }

            if stem.hasSuffix("(") {
                stem.removeLast()
            }

            // The random token is the part after the last `~`; the base name is everything before it.
            if let lastTilde = stem.lastIndex(of: "~") {
                stem = String(stem[..<lastTilde])
            }

            return stem.isEmpty ? nil : stem
        case "prlock":
            // Premiere Pro: `{base name}.prlock`.
            return stem.isEmpty ? nil : stem
        default:
            return nil
    }
}

///
/// Resolve the document guarded by an Adobe lock file by matching a sibling file in the same
/// directory by base name and expected document extension.
///
/// - Parameters:
///     - lockFilename: The Adobe lock file name.
///     - parentServerUrl: The server URL of the directory containing the lock file.
///     - dbManager: The database manager to use for looking up sibling files.
///
/// - Returns: The guarded document's file name, or `nil` if no matching document is found.
///
func adobeLockFileTargetName(lockFilename: String, parentServerUrl: String, dbManager: FilesDatabaseManager) -> String? {
    let ext = (lockFilename as NSString).pathExtension.lowercased()

    guard let documentExtensions = adobeLockFileDocumentExtensions[ext],
          let baseName = adobeLockFileDocumentBaseName(lockFilename)
    else {
        return nil
    }

    // Prefer the first matching extension, e.g. `.indd` over `.icml` for `.idlk`.
    for documentExtension in documentExtensions {
        let candidate = baseName + "." + documentExtension

        if dbManager.itemMetadatas
            .where({ $0.serverUrl.equals(parentServerUrl) })
            .where({ $0.fileName.equals(candidate) })
            .first != nil
        {
            return candidate
        }
    }

    return nil
}

///
/// Resolve the document guarded by an AutoCAD lock file.
///
/// AutoCAD lock files (`.dwl` / `.dwl2`) share the exact same base name as the guarded
/// `.dwg` document — only the extension differs — so the document name is derived by
/// replacing the lock extension with `.dwg`. No sibling lookup is needed.
///
/// - Parameters:
///     - lockFilename: The AutoCAD lock file name.
///
/// - Returns: The guarded document's file name, or `nil` if it is not an AutoCAD lock file.
///
func autoCADLockFileTargetName(_ lockFilename: String) -> String? {
    guard isAutoCADLockFileName(lockFilename) else { return nil }
    let baseName = (lockFilename as NSString).deletingPathExtension
    return baseName.isEmpty ? nil : baseName + "." + autoCADDocumentExtension
}

///
/// Resolve the document guarded by a lock file, regardless of the application that created it.
///
/// Office and LibreOffice lock file names fully encode the document name, so it is decoded
/// directly via ``originalFileName(fromLockFileName:dbManager:)``. Adobe lock file names only
/// encode the base name, so the document is resolved by matching a sibling file via
/// ``adobeLockFileTargetName(lockFilename:parentServerUrl:dbManager:)``. AutoCAD lock files
/// share the document's base name, so the document is resolved by replacing the extension
/// with `.dwg` via ``autoCADLockFileTargetName(_:)``.
///
/// - Parameters:
///     - lockFilename: The lock file name.
///     - parentServerUrl: The server URL of the directory containing the lock file.
///     - dbManager: The database manager to use for looking up files.
///
/// - Returns: The guarded document's file name, or `nil` if it cannot be determined.
///
public func lockFileTargetName(forLockFileName lockFilename: String, parentServerUrl: String, dbManager: FilesDatabaseManager) -> String? {
    if isAdobeLockFileName(lockFilename) {
        return adobeLockFileTargetName(lockFilename: lockFilename, parentServerUrl: parentServerUrl, dbManager: dbManager)
    }

    if isAutoCADLockFileName(lockFilename) {
        return autoCADLockFileTargetName(lockFilename)
    }

    return originalFileName(fromLockFileName: lockFilename, dbManager: dbManager)
}
