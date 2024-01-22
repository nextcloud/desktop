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

func pathForAppGroupContainer() -> URL? {
    guard
        let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix")
            as? String
    else {
        Logger.localFileOps.critical(
            "Could not get container url as missing SocketApiPrefix info in app Info.plist")
        return nil
    }

    return FileManager.default.containerURL(
        forSecurityApplicationGroupIdentifier: appGroupIdentifier)
}

func pathForFileProviderExtData() -> URL? {
    let containerUrl = pathForAppGroupContainer()
    return containerUrl?.appendingPathComponent("FileProviderExt/")
}

func pathForFileProviderTempFilesForDomain(_ domain: NSFileProviderDomain) throws -> URL? {
    guard let fpManager = NSFileProviderManager(for: domain) else {
        Logger.localFileOps.error(
            "Unable to get file provider manager for domain: \(domain.displayName, privacy: .public)"
        )
        throw NSFileProviderError(.providerNotFound)
    }

    let fileProviderDataUrl = try fpManager.temporaryDirectoryURL()
    return fileProviderDataUrl.appendingPathComponent("TemporaryNextcloudFiles/")
}

func localPathForNCFile(ocId _: String, fileNameView: String, domain: NSFileProviderDomain) throws
    -> URL
{
    guard let fileProviderFilesPathUrl = try pathForFileProviderTempFilesForDomain(domain) else {
        Logger.localFileOps.error(
            "Unable to get path for file provider temp files for domain: \(domain.displayName, privacy: .public)"
        )
        throw URLError(.badURL)
    }

    let filePathUrl = fileProviderFilesPathUrl.appendingPathComponent(fileNameView)
    let filePath = filePathUrl.path

    if !FileManager.default.fileExists(atPath: filePath) {
        FileManager.default.createFile(atPath: filePath, contents: nil)
    }

    return filePathUrl
}
