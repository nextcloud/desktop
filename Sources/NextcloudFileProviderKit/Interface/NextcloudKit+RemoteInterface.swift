//
//  NextcloudKit+RemoteInterface.swift
//  
//
//  Created by Claudio Cambra on 16/4/24.
//

import Alamofire
import FileProvider
import Foundation
import NextcloudKit

extension NextcloudKit: RemoteInterface {

    public var account: Account {
        Account(
            user: nkCommonInstance.user,
            serverUrl: nkCommonInstance.urlBase,
            password: nkCommonInstance.password
        )
    }

    public func createFolder(
        remotePath: String,
        options: RemoteRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NSFileProviderError?) {
        return await withCheckedContinuation { continuation in
            createFolder(
                serverUrlFileName: remotePath,
                options: options.toNKRequestOptions(),
                taskHandler: taskHandler
            ) { account, ocId, date, error in
                continuation.resume(returning: (account, ocId, date, error.fileProviderError))
            }
        }
    }

    public func upload(
        remotePath: String,
        localPath: String, 
        creationDate: Date? = nil,
        modificationDate: Date? = nil,
        options: RemoteRequestOptions = .init(),
        requestHandler: @escaping (UploadRequest) -> Void = { _ in },
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
        progressHandler: @escaping (Progress) -> Void = { _ in }
    ) async -> (
        account: String,
        ocId: String?,
        etag: String?,
        date: NSDate?,
        size: Int64,
        allHeaderFields: [AnyHashable : Any]?,
        afError: AFError?,
        remoteError: NSFileProviderError?
    ) {
        return await withCheckedContinuation { continuation in
            upload(
                serverUrlFileName: remotePath,
                fileNameLocalPath: localPath,
                dateCreationFile: creationDate,
                dateModificationFile: modificationDate,
                options: options.toNKRequestOptions(),
                requestHandler: requestHandler,
                taskHandler: taskHandler,
                progressHandler: progressHandler
            ) { account, ocId, etag, date, size, allHeaderFields, afError, nkError in
                continuation.resume(returning: (
                    account,
                    ocId,
                    etag,
                    date,
                    size,
                    allHeaderFields,
                    afError,
                    nkError.fileProviderError
                ))
            }
        }
    }

    public func download(
        remotePath: String,
        localPath: String,
        options: RemoteRequestOptions = .init(),
        requestHandler: @escaping (DownloadRequest) -> Void = { _ in },
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
        progressHandler: @escaping (Progress) -> Void = { _ in }
    ) async -> (
        account: String,
        etag: String?,
        date: NSDate?,
        length: Int64,
        allHeaderFields: [AnyHashable : Any]?,
        afError: AFError?,
        remoteError: NSFileProviderError?
    ) {
        return await withCheckedContinuation { continuation in
            download(
                serverUrlFileName: remotePath,
                fileNameLocalPath: localPath,
                options: options.toNKRequestOptions(),
                requestHandler: requestHandler,
                taskHandler: taskHandler,
                progressHandler: progressHandler
            ) { account, etag, date, length, allHeaderFields, afError, remoteError in
                continuation.resume(returning: (
                    account, 
                    etag,
                    date,
                    length,
                    allHeaderFields,
                    afError,
                    remoteError.fileProviderError
                ))
            }
        }
    }

    public func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool = false,
        includeHiddenFiles: [String] = [],
        requestBody: Data? = nil,
        options: RemoteRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (
        account: String, files: [RemoteFileMetadata], data: Data?, error: NSFileProviderError?
    ) {
        return await withCheckedContinuation { continuation in
            readFileOrFolder(
                serverUrlFileName: remotePath,
                depth: depth.rawValue,
                showHiddenFiles: showHiddenFiles,
                includeHiddenFiles: includeHiddenFiles,
                requestBody: requestBody,
                options: options.toNKRequestOptions(),
                taskHandler: taskHandler
            ) { account, files, data, error in
                continuation.resume(returning: (account, files, data, error.fileProviderError))
            }
        }
    }

    public func delete(
        remotePath: String,
        options: RemoteRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, error: NSFileProviderError?) {
        return await withCheckedContinuation { continuation in
            deleteFileOrFolder(serverUrlFileName: remotePath) { account, error in
                continuation.resume(returning: (account, error.fileProviderError))
            }
        }
    }

    public func downloadThumbnail(
        url: URL, options: RemoteRequestOptions, taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NSFileProviderError?) {
        await withCheckedContinuation { continuation in
            getPreview(
                url: url, options: options.toNKRequestOptions(), taskHandler: taskHandler
            ) { account, data, error in
                continuation.resume(returning: (account, data, error.fileProviderError))
            }
        }
    }

    public func getInternalType(
        fileName: String, mimeType: String, directory: Bool
    ) -> (
        mimeType: String,
        classFile: String,
        iconName: String,
        typeIdentifier: String,
        fileNameWithoutExt: String,
        ext: String
    ) {
        return nkCommonInstance.getInternalType(
            fileName: fileName, mimeType: mimeType, directory: directory
        )
    }
}
