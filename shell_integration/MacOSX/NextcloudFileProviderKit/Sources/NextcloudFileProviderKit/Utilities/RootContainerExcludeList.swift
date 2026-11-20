//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

///
/// The patterns of the `.sync-exclude.lst` file in the root container, which apply to the whole domain as in the classic sync engine.
///
/// The local database tells whether the file exists and which version it has. The patterns are cached per version and are runtime-only. Exclude list files in other folders are not read.
///
actor RootContainerExcludeList {
    private let log: any FileProviderLogging
    private let logger: FileProviderLogger
    private var cached: (etag: String, matcher: IgnoredFilesMatcher)?
    private var downloads = [String: Task<IgnoredFilesMatcher, any Error>]()

    init(log: any FileProviderLogging) {
        self.log = log
        logger = FileProviderLogger(category: "RootContainerExcludeList", log: log)
    }

    ///
    /// The remote path of the exclude list file in the root container.
    ///
    static func remotePath(account: Account) -> String {
        account.davFilesUrl + "/" + IgnoredFilesMatcher.excludeListFileName
    }

    ///
    /// `ignoredFiles` extended by the patterns of the exclude list file in the root container, or `ignoredFiles` alone if there is no such file.
    ///
    /// Concurrent callers share one download of the same version.
    ///
    /// - Throws: An `NSFileProviderError` that the system retries on when the file exists but no version of it could be read.
    ///
    func matcher(extending ignoredFiles: IgnoredFilesMatcher, account: Account, remoteInterface: any RemoteInterface, dbManager: FilesDatabaseManager) async throws -> IgnoredFilesMatcher {
        let remotePath = Self.remotePath(account: account)

        // An item without an etag was never uploaded, so there is nothing on the server to read.
        guard let metadata = dbManager.itemMetadata(account: account.ncKitAccount, locatedAtRemoteUrl: remotePath),
              !metadata.deleted,
              !metadata.directory,
              !metadata.etag.isEmpty
        else {
            return ignoredFiles
        }

        if let cached, cached.etag == metadata.etag {
            return ignoredFiles.merging(cached.matcher)
        }

        do {
            return try await ignoredFiles.merging(download(etag: metadata.etag, remotePath: remotePath, account: account, remoteInterface: remoteInterface))
        } catch {
            guard let cached else {
                logger.error("Exclude list in the root container could not be read.", [.eTag: metadata.etag, .error: error])
                throw error
            }

            logger.error("Using an earlier version of the exclude list in the root container because the current one could not be read.", [.eTag: metadata.etag, .error: error])
            return ignoredFiles.merging(cached.matcher)
        }
    }

    ///
    /// Cache the patterns of the exclude list file in the root container after uploading it, so they are not downloaded again.
    ///
    /// - Parameters:
    ///     - metadata: The metadata of an uploaded item. Items other than the exclude list file in the root container are ignored.
    ///     - contents: The local file the contents were uploaded from.
    ///
    func didUpload(_ metadata: SendableItemMetadata, contents: URL?, account: Account) {
        guard metadata.remotePath() == Self.remotePath(account: account), !metadata.directory, !metadata.etag.isEmpty, let contents else {
            return
        }

        guard let data = try? Data(contentsOf: contents) else {
            logger.error("Could not read the uploaded exclude list in the root container, so it will be downloaded when needed.", [.url: contents])
            return
        }

        cached = (metadata.etag, makeMatcher(data))
    }

    private func download(etag: String, remotePath: String, account: Account, remoteInterface: any RemoteInterface) async throws -> IgnoredFilesMatcher {
        if let download = downloads[etag] {
            return try await download.value
        }

        let download = Task {
            let data = try await Self.downloadData(remotePath: remotePath, account: account, remoteInterface: remoteInterface)
            return makeMatcher(data)
        }

        downloads[etag] = download
        defer { downloads[etag] = nil }

        let matcher = try await download.value
        cached = (etag, matcher)
        logger.info("Downloaded exclude list in the root container.", [.eTag: etag])

        return matcher
    }

    private static func downloadData(remotePath: String, account: Account, remoteInterface: any RemoteInterface) async throws -> Data {
        let localUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: localUrl) }

        let (_, _, error) = await remoteInterface.downloadAsync(
            serverUrlFileName: remotePath,
            fileNameLocalPath: localUrl.path,
            account: account.ncKitAccount,
            options: .init(),
            requestHandler: { _ in },
            taskHandler: { _ in },
            progressHandler: { _ in }
        )

        // The database has not caught up with a removal on the server yet, and a missing list excludes nothing.
        if error.isNotFoundError {
            return Data()
        }

        guard error == .success else {
            // Keep only the errors after which the system retries once the cause is resolved.
            switch error.fileProviderError?.code {
                case .serverUnreachable?, .notAuthenticated?:
                    throw error.fileProviderError ?? NSFileProviderError(.cannotSynchronize)
                default:
                    throw NSFileProviderError(.cannotSynchronize)
            }
        }

        guard let data = try? Data(contentsOf: localUrl) else {
            throw NSFileProviderError(.cannotSynchronize)
        }

        return data
    }

    private func makeMatcher(_ data: Data) -> IgnoredFilesMatcher {
        // Invalid UTF-8 is replaced rather than rejected, like `QString::fromUtf8` in the classic sync engine.
        let contents = String(decoding: data, as: UTF8.self)
        return IgnoredFilesMatcher(ignoreList: IgnoredFilesMatcher.patterns(inExcludeList: contents), log: log)
    }
}
