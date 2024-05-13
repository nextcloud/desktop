//
//  MockRemoteInterface.swift
//
//
//  Created by Claudio Cambra on 9/5/24.
//

import Alamofire
import Foundation
import NextcloudFileProviderKit
import NextcloudKit

public class MockRemoteInterface: RemoteInterface {
    public var account: Account
    public var rootItem: MockRemoteItem?
    public var delegate: (any NKCommonDelegate)?

    private var accountString: String { account.ncKitAccount }

    public init(account: Account, rootItem: MockRemoteItem? = nil) {
        self.account = account
        self.rootItem = rootItem
    }

    func item(remotePath: String) -> MockRemoteItem? {
        guard let rootItem else { return nil }
        var currentNode = rootItem
        guard remotePath != "/" else { return currentNode }

        var pathComponents = remotePath.components(separatedBy: "/")
        if pathComponents.first?.isEmpty == true { pathComponents.removeFirst() }

        while !pathComponents.isEmpty {
            let component = pathComponents.removeFirst()
            guard !component.isEmpty,
                  let nextNode = currentNode.children.first(where: { $0.name == component })
            else { return nil }

            guard !pathComponents.isEmpty else { return nextNode } // This is the target
            currentNode = nextNode
        }

        return nil
    }

    func parentPath(path: String) -> String {
        var pathComponents = path.components(separatedBy: "/")
        if pathComponents.first?.isEmpty == true { pathComponents.removeFirst() }
        guard !pathComponents.isEmpty else { return "/" }
        pathComponents.removeLast()
        return "/" + pathComponents.joined(separator: "/")
    }

    func parentItem(path: String) -> MockRemoteItem? {
        let parentRemotePath = parentPath(path: path)
        return item(remotePath: parentRemotePath)
    }

    func randomIdentifier() -> String {
        UUID().uuidString
    }

    public func setDelegate(_ delegate: any NKCommonDelegate) {
        self.delegate = delegate
    }

    public func createFolder(
        remotePath: String,
        options: NKRequestOptions = .init(),
taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError) {
        guard !remotePath.isEmpty else { return (accountString, nil, nil, .urlError) }
        let splitPath = remotePath.split(separator: "/")
        let name = String(splitPath.last!)
        guard !name.isEmpty else { return (accountString, nil, nil, .urlError) }
        let item = MockRemoteItem(
            identifier: randomIdentifier(), name: name, directory: true
        )
        let parent = parentItem(path: remotePath)
        parent?.children.append(item)
        item.parent = parent
        return (accountString, item.identifier, item.creationDate as NSDate, .success)
    }

    public func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date?,
        modificationDate: Date?,
        options: NKRequestOptions,
        requestHandler: @escaping (Alamofire.UploadRequest) -> Void,
        taskHandler: @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void
    ) async -> (
        account: String,
        ocId: String?,
        etag: String?,
        date: NSDate?,
        size: Int64,
        allHeaderFields: [AnyHashable : Any]?,
        afError: AFError?,
        remoteError: NKError
    ) {

        let itemIdentifier = "" // TODO: Implement upload
        let itemVersion = ""
        let itemSize = Int64(0)
        return (accountString, itemIdentifier, itemVersion, NSDate(), itemSize, nil, nil, .success)
    }

    public func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, error: NKError) {
        // TODO: Implement move
        return (accountString, .success)
    }

    public func download(
        remotePath: String,
        localPath: String,
        options: NKRequestOptions,
        requestHandler: @escaping (DownloadRequest) -> Void,
        taskHandler: @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void
    ) async -> (
        account: String,
        etag: String?,
        date: NSDate?,
        length: Int64,
        allHeaderFields: [AnyHashable : Any]?,
        afError: AFError?,
        remoteError: NKError
    ) {
        // TODO: Implement download
        return (accountString, "", NSDate(), 0, nil, nil, .success)
    }

    public func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool,
        includeHiddenFiles: [String],
        requestBody: Data?,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, files: [NKFile], data: Data?, error: NKError) {
        // TODO: Implement enumerate
        return (accountString, [], nil, .success)
    }

    public func delete(
        remotePath: String,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, error: NKError) {
        // TODO: Implement delete
        return (accountString, .success)
    }

    public func downloadThumbnail(
        url: URL,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        // TODO: Implement downloadThumbnail
        return (accountString, nil, .success)
    }

    public func fetchCapabilities(
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: 	NKError) {
        // TODO: Implement fetchCapabilities
        return (accountString, nil, .success)
    }
}
