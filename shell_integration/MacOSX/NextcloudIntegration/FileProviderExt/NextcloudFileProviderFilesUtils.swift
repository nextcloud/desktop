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

import Foundation
import FileProvider

func pathForAppGroupContainer() -> URL? {
    guard let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String else {
        print("Could not get container url as missing SocketApiPrefix info in app Info.plist")
        return nil
    }

    return FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: appGroupIdentifier)
}

func pathForFileProviderExtData() -> URL? {
    let containerUrl = pathForAppGroupContainer()
    return containerUrl?.appendingPathExtension("FileProviderExt/")
}

func pathForFileProviderExtFiles() -> URL? {
    let fileProviderDataUrl = pathForFileProviderExtData()
    return fileProviderDataUrl?.appendingPathExtension("Files/")
}

@discardableResult func localPathForNCDirectory(ocId: String) throws -> URL {
    guard let fileProviderFilesPathUrl = pathForFileProviderExtFiles() else {
        throw URLError(.badURL)
    }

    let folderPathUrl = fileProviderFilesPathUrl.appendingPathExtension(ocId)
    let folderPath = folderPathUrl.path

    if !FileManager.default.fileExists(atPath: folderPath) {
        try FileManager.default.createDirectory(at: folderPathUrl, withIntermediateDirectories: true)
    }

    return folderPathUrl
}

@discardableResult func localPathForNCDirectory(directoryMetadata: NextcloudDirectoryMetadataTable) throws -> URL {
    let ocId = directoryMetadata.ocId
    return try localPathForNCDirectory(ocId: ocId)
}

@discardableResult func localPathForNCDirectory(fileMetadata: NextcloudFileMetadataTable) throws -> URL {
    let ocId = fileMetadata.ocId
    return try localPathForNCDirectory(ocId: ocId)
}

@discardableResult func localPathForNCFile(ocId: String, fileNameView: String) throws -> URL {
    let fileFolderPathUrl = try localPathForNCDirectory(ocId: ocId)
    let filePathUrl = fileFolderPathUrl.appendingPathExtension(fileNameView)
    let filePath = filePathUrl.path

    if !FileManager.default.fileExists(atPath: filePath) {
        FileManager.default.createFile(atPath: filePath, contents: nil)
    }

    return filePathUrl
}

@discardableResult func localPathForNCFile(fileMetadata: NextcloudFileMetadataTable) throws -> URL {
    let ocId = fileMetadata.ocId
    let fileNameView = fileMetadata.fileNameView
    return try localPathForNCFile(ocId: ocId, fileNameView: fileNameView)
}

func createFileOrDirectoryLocally(metadata: NextcloudFileMetadataTable) {
    do {
        if metadata.directory {
            try localPathForNCDirectory(ocId: metadata.ocId)
        } else {
            try localPathForNCFile(fileMetadata: metadata)
        }
    } catch let error {
        print("Could not create NC file or directory locally, received error: %@", error)
    }
}

func parentItemIdentifierFromMetadata(_ metadata: NextcloudFileMetadataTable) -> NSFileProviderItemIdentifier? {
    let homeServerFilesUrl = metadata.urlBase + "/remote.php/dav/files/" + metadata.userId
    let ncDatabase = NextcloudFilesDatabaseManager.shared

    guard let itemParentDirectory = ncDatabase.directoryMetadataForFile(metadata) else { return nil }
    if itemParentDirectory.serverUrl == homeServerFilesUrl {
        return .rootContainer
    } else if let parentDirectoryMetadata = ncDatabase.fileMetadataFromOcId(itemParentDirectory.ocId) {
        return NSFileProviderItemIdentifier(parentDirectoryMetadata.ocId)
    }

    return nil
}
