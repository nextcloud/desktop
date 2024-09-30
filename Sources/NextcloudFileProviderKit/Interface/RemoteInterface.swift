//
//  RemoteInterface.swift
//
//
//  Created by Claudio Cambra on 16/4/24.
//

import Alamofire
import FileProvider
import Foundation
import NextcloudKit

public enum EnumerateDepth: String {
    case target = "0"
    case targetAndDirectChildren = "1"
    case targetAndAllChildren = "infinity"
}

public protocol RemoteInterface {

    var account: Account { get }

    func setDelegate(_ delegate: NKCommonDelegate)

    func createFolder(
        remotePath: String,
        options: NKRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError)

    func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date?,
        modificationDate: Date?,
        options: NKRequestOptions,
        requestHandler: @escaping (_ request: UploadRequest) -> Void,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void,
        progressHandler: @escaping (_ progress: Progress) -> Void
    ) async -> (
        account: String,
        ocId: String?,
        etag: String?,
        date: NSDate?,
        size: Int64,
        allHeaderFields: [AnyHashable: Any]?,
        afError: AFError?, 
        remoteError: NKError
    )

    func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool,
        options: NKRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, error: NKError)

    func download(
        remotePath: String,
        localPath: String,
        options: NKRequestOptions,
        requestHandler: @escaping (_ request: DownloadRequest) -> Void,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void,
        progressHandler: @escaping (_ progress: Progress) -> Void
    ) async -> (
        account: String,
        etag: String?,
        date: NSDate?,
        length: Int64,
        allHeaderFields: [AnyHashable: Any]?,
        afError: AFError?,
        remoteError: NKError
    )

    func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool,
        includeHiddenFiles: [String],
        requestBody: Data?,
        options: NKRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, files: [NKFile], data: Data?, error: NKError)

    func delete(
        remotePath: String,
        options: NKRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, error: NKError)

    func downloadThumbnail(
        url: URL,
        options: NKRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError)

    func fetchCapabilities(
        options: NKRequestOptions, taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError)

    func fetchUserProfile(
        options: NKRequestOptions, taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, userProfile: NKUserProfile?, data: Data?, error: NKError)
}
