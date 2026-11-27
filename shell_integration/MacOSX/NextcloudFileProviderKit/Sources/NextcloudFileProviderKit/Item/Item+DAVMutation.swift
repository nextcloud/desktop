//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

extension Item {
    /// A cached URL can now point to a different file or folder. Read just that URL and
    /// compare its server ID before sending a write. Leave the cache to normal enumeration.
    /// Return the current ETag, or nil if the server reports the URL is missing.
    static func confirmDAVItemAtMutationPath(
        _ metadata: SendableItemMetadata,
        account: Account,
        remoteInterface: RemoteInterface,
        domain: NSFileProviderDomain? = nil
    ) async throws -> String? {
        func filesItemPath(_ url: String) -> String? {
            guard let path = account.filesRootRelativePath(for: url) else { return nil }
            let components = path.split(separator: "/")
            guard !components.isEmpty,
                  !components.contains("..")
            else { return nil }
            return path
        }

        let remotePath = metadata.remotePath()
        // The parser's URL can also be the literal URL of a previous account configuration.
        guard metadata.urlBase.utf8.elementsEqual(account.serverUrl.utf8),
              metadata.userId.utf8.elementsEqual(account.id.utf8),
              let requestedPath = filesItemPath(remotePath)
        else {
            throw NSFileProviderError(.serverUnreachable)
        }

        try Task.checkCancellation()
        let (_, files, _, error) = await remoteInterface.enumerate(
            remotePath: remotePath,
            depth: .target,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: .init(),
            taskHandler: { task in
                if let domain {
                    NSFileProviderManager(for: domain)?.register(
                        task,
                        forItemWithIdentifier: .init(metadata.ocId),
                        completionHandler: { _ in }
                    )
                }
            }
        )
        try Task.checkCancellation()
        if error.isNotFoundError {
            return nil
        }
        guard error == .success else {
            if error.isCancelledError {
                throw CocoaError(.userCancelled)
            }
            throw error.fileProviderError ?? NSFileProviderError(.serverUnreachable)
        }

        guard files.count == 1,
              let file = files.first,
              file.ocId.utf8.elementsEqual(metadata.ocId.utf8),
              file.directory == metadata.directory,
              let responsePath = filesItemPath(file.serverUrl + "/" + file.fileName),
              responsePath.utf8.elementsEqual(requestedPath.utf8)
        else { throw NSFileProviderError(.serverUnreachable) }

        return file.etag
    }

    /// SabreDAV checks If-Match ETags on files. It rejects the same precondition on folders.
    static func ifMatchOptions(forETag etag: String, isDirectory: Bool) -> NKRequestOptions {
        guard !isDirectory, !etag.isEmpty else { return .init() }
        return .init(customHeader: ["If-Match": "\"\(etag)\""])
    }

    /// Let File Provider retry the operation, which reads the current item and ETag again.
    static func retryableDAVWriteConflictError(_ error: NKError) -> CocoaError {
        CocoaError(.fileWriteUnknown, userInfo: [NSUnderlyingErrorKey: error])
    }
}
