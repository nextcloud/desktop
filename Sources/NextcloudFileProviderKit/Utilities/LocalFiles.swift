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

public func pathForFileProviderExtData() -> URL? {
    let containerUrl = pathForAppGroupContainer()
    return containerUrl?.appendingPathComponent("FileProviderExt/")
}

public func pathForFileProviderTempFilesForDomain(_ domain: NSFileProviderDomain) throws -> URL? {
    guard let fpManager = NSFileProviderManager(for: domain) else {
        lfuLogger.error(
            "Unable to get file provider manager for domain: \(domain.displayName, privacy: .public)"
        )
        throw NSFileProviderError(.providerNotFound)
    }

    let fileProviderDataUrl = try fpManager.temporaryDirectoryURL()
    return fileProviderDataUrl.appendingPathComponent("TemporaryNextcloudFiles/")
}

public func isLockFileName(_ filename: String) -> Bool {
    // Microsoft Office lock files
    return filename.hasPrefix("~$") ||
        // LibreOffice lock files
        (filename.hasPrefix(".~lock.") && filename.hasSuffix("#"))
}

public func originalFilename(fromLockFilename lockFilename: String) -> String? {
    // Microsoft Office: "~$MyDoc.docx" -> "MyDoc.docx"
    if lockFilename.hasPrefix("~$") {
        // Remove the "~$" prefix
        let index = lockFilename.index(lockFilename.startIndex, offsetBy: 2)
        return String(lockFilename[index...])
    }

    // LibreOffice: ".~lock.MyDoc.odt#" -> "MyDoc.odt"
    if lockFilename.hasPrefix(".~lock.") && lockFilename.hasSuffix("#") {
        // Strip the prefix and suffix
        let start = lockFilename.index(lockFilename.startIndex, offsetBy: 7)
        let end = lockFilename.index(before: lockFilename.endIndex)
        return String(lockFilename[start..<end])
    }

    // Not a recognized lock file
    return nil
}
