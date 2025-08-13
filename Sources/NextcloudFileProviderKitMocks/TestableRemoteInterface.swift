//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Alamofire
import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import NextcloudKit

public struct TestableRemoteInterface: RemoteInterface {
    public init() {}

    public func setDelegate(_ delegate: any NextcloudKitDelegate) {}

    public func createFolder(
        remotePath: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError) {
        ("", nil, nil, .invalidResponseError)
    }

    public func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date?,
        modificationDate: Date?,
        account: Account,
        options: NKRequestOptions,
        requestHandler: @escaping (UploadRequest) -> Void,
        taskHandler: @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void
    ) async -> (
        account: String,
        ocId: String?,
        etag: String?,
        date: NSDate?,
        size: Int64,
        response: HTTPURLResponse?,
        remoteError: NKError
    ) { ("", nil, nil, nil, 0, nil, .invalidResponseError) }

    public func chunkedUpload(
        localPath: String,
        remotePath: String,
        remoteChunkStoreFolderName: String,
        chunkSize: Int,
        remainingChunks: [RemoteFileChunk],
        creationDate: Date?,
        modificationDate: Date?,
        account: Account,
        options: NKRequestOptions,
        currentNumChunksUpdateHandler: @escaping (Int) -> Void,
        chunkCounter: @escaping (Int) -> Void,
        log: any FileProviderLogging,
        chunkUploadStartHandler: @escaping ([RemoteFileChunk]) -> Void,
        requestHandler: @escaping (UploadRequest) -> Void,
        taskHandler: @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void,
        chunkUploadCompleteHandler: @escaping (RemoteFileChunk) -> Void
    ) async -> (
        account: String,
        fileChunks: [RemoteFileChunk]?,
        file: NKFile?,
        nkError: NKError
    ) {
        ("", nil, nil, .invalidResponseError)
    }

    public func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) { ("", nil, .invalidResponseError) }

    public func download(
        remotePath: String,
        localPath: String,
        account: Account,
        options: NKRequestOptions,
        requestHandler: @escaping (DownloadRequest) -> Void,
        taskHandler: @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void
    ) async -> (
        account: String,
        etag: String?,
        date: NSDate?,
        length: Int64,
        headers: [AnyHashable: Any]?,
        afError: AFError?,
        nkError: NKError
    ) {
        ("", nil, nil, 0, nil, nil, .invalidResponseError)
    }

    public func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool,
        includeHiddenFiles: [String],
        requestBody: Data?,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, files: [NKFile], data: AFDataResponse<Data>?, error: NKError) {
        ("", [], nil, .invalidResponseError)
    }

    public func delete(
        remotePath: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, response: HTTPURLResponse?, error: NKError) {
        ("", nil, .invalidResponseError)
    }

    public func setLockStateForFile(
        remotePath: String,
        lock: Bool,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, response: HTTPURLResponse?, error: NKError) {
        ("", nil, .invalidResponseError)
    }

    public func trashedItems(
        account: Account, options: NKRequestOptions, taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, trashedItems: [NKTrash], data: Data?, error: NKError) {
        ("", [], nil, .invalidResponseError)
    }

    public func restoreFromTrash(
        filename: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) { ("", nil, .invalidResponseError) }

    public func downloadThumbnail(
        url: URL,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) { ("", nil, .invalidResponseError) }

    public func fetchUserProfile(
        account: Account, options: NKRequestOptions, taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, userProfile: NKUserProfile?, data: Data?, error: NKError) {
        ("", nil, nil, .invalidResponseError)
    }

    public func tryAuthenticationAttempt(
        account: Account, options: NKRequestOptions, taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> AuthenticationAttemptResultState { .connectionError }

    public typealias FetchResult = (account: String, capabilities: Capabilities?, data: Data?, error: NKError)

    public var fetchCapabilitiesHandler:
        ((Account, NKRequestOptions, @escaping (URLSessionTask) -> Void) async -> FetchResult)?

    public func fetchCapabilities(
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (_ task: URLSessionTask) -> Void = { _ in }
    ) async -> FetchResult {
        let ncKitAccount = account.ncKitAccount
        await RetrievedCapabilitiesActor.shared.setOngoingFetch(
            forAccount: ncKitAccount, ongoing: true
        )
        var response: FetchResult
        if let handler = fetchCapabilitiesHandler {
            response = await handler(account, options, taskHandler)
            if let caps = response.capabilities {
                await RetrievedCapabilitiesActor.shared.setCapabilities(
                    forAccount: ncKitAccount, capabilities: caps, retrievedAt: Date()
                )
            }
        } else {
            print("Error: fetchCapabilitiesHandler not set in TestableRemoteInterface")
            response = (account.ncKitAccount, nil, nil, .invalidResponseError)
        }
        await RetrievedCapabilitiesActor.shared.setOngoingFetch(
            forAccount: account.ncKitAccount, ongoing: false
        )
        return response
    }
}
