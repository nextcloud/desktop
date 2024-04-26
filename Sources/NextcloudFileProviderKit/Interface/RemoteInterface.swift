//
//  RemoteInterface.swift
//
//
//  Created by Claudio Cambra on 16/4/24.
//

import Alamofire
import Foundation

public enum EnumerateDepth: String {
    case target = "0"
    case targetAndDirectChildren = "1"
    case targetAndAllChildren = "infinity"
}

public protocol RemoteInterface {

    var account: Account { get }

    func createFolder(
        remotePath: String,
        options: RemoteRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, ocId: String?, date: NSDate?, error: Error?)

    func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date?,
        modificationDate: Date?,
        options: RemoteRequestOptions,
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
        remoteError: Error?
    )

    func download(
        remotePath: String,
        localPath: String,
        options: RemoteRequestOptions,
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
        remoteError: Error?
    )

    func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool,
        includeHiddenFiles: [String],
        requestBody: Data?,
        options: RemoteRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, files: [RemoteFileMetadata], data: Data?, error: Error?)

    func delete(
        remotePath: String,
        options: RemoteRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, error: Error?)

    func downloadThumbnail(
        url: URL,
        options: RemoteRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: Error?)

    func getInternalType(
        fileName: String,
        mimeType: String, directory: Bool
    ) -> (
        mimeType: String,
        classFile: String,
        iconName: String,
        typeIdentifier: String,
        fileNameWithoutExt: String,
        ext: String
    )
}
