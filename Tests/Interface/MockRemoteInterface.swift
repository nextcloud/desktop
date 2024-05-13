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

    func sanitisedPath(_ path: String) -> String {
        var sanitisedPath = path
        let filesPath = account.davFilesUrl
        if sanitisedPath.hasPrefix(filesPath) {
            // Keep the leading slash for root path
            let trimCount = filesPath.last == "/" ? filesPath.count - 1 : filesPath.count
            sanitisedPath = String(sanitisedPath.dropFirst(trimCount))
        }
        if sanitisedPath != "/", sanitisedPath.last == "/" {
            sanitisedPath = String(sanitisedPath.dropLast())
        }
        return sanitisedPath
    }

    func item(remotePath: String) -> MockRemoteItem? {
        guard let rootItem, !remotePath.isEmpty else { return nil }

        let sanitisedPath = sanitisedPath(remotePath)
        print(remotePath, sanitisedPath)
        guard sanitisedPath != "/" else { return rootItem }

        var pathComponents = sanitisedPath.components(separatedBy: "/")
        if pathComponents.first?.isEmpty == true { pathComponents.removeFirst() }
        var currentNode = rootItem

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
        let sanitisedPath = sanitisedPath(path)
        var pathComponents = sanitisedPath.components(separatedBy: "/")
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

    func name(from path: String) throws -> String {
        guard !path.isEmpty else { throw URLError(.badURL) }

        let sanitisedPath = path.last == "/" ? String(path.dropLast()) : path
        let splitPath = sanitisedPath.split(separator: "/")
        let name = String(splitPath.last!)
        guard !name.isEmpty else { throw URLError(.badURL) }

        return name
    }

    public func setDelegate(_ delegate: any NKCommonDelegate) {
        self.delegate = delegate
    }

    public func createFolder(
        remotePath: String,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError) {
        var itemName: String
        do {
            itemName = try name(from: remotePath)
        } catch {
            return (accountString, nil, nil, .urlError)
        }

        let item = MockRemoteItem(
            identifier: randomIdentifier(), name: itemName, directory: true
        )
        guard let parent = parentItem(path: remotePath) else {
            return (accountString, nil, nil, .urlError)
        }

        parent.children.append(item)
        item.parent = parent
        return (accountString, item.identifier, item.creationDate as NSDate, .success)
    }

    public func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date? = .init(),
        modificationDate: Date? = .init(),
        options: NKRequestOptions = .init(),
        requestHandler: @escaping (Alamofire.UploadRequest) -> Void = { _ in },
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
        remoteError: NKError
    ) {
        var itemName: String
        do {
            itemName = try name(from: localPath)
        } catch {
            return (accountString, nil, nil, nil, 0, nil, nil, .urlError)
        }

        let itemLocalUrl = URL(fileURLWithPath: localPath)
        var itemData: Data
        do {
            itemData = try Data(contentsOf: itemLocalUrl)
        } catch {
            return (accountString, nil, nil, nil, 0, nil, nil, .urlError)
        }

        let item = MockRemoteItem(
            identifier: randomIdentifier(), name: itemName, data: itemData
        )
        guard let parent = parentItem(path: remotePath) else {
            return (accountString, nil, nil, nil, 0, nil, nil, .urlError)
        }

        parent.children.append(item)
        item.parent = parent

        return (
            accountString,
            item.identifier,
            item.versionIdentifier,
            item.creationDate as NSDate,
            item.size,
            nil,
            nil,
            .success
        )
    }

    public func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool = false,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, error: NKError) {
        guard let itemNewName = try? name(from: remotePathDestination),
              let sourceItem = item(remotePath: remotePathSource),
              let destinationParent = parentItem(path: remotePathDestination),
              (overwrite || !destinationParent.children.contains(where: { $0.name == itemNewName }))
        else { return (accountString, .urlError) }

        sourceItem.name = itemNewName
        sourceItem.parent?.children.removeAll(where: { $0.identifier == sourceItem.identifier })
        sourceItem.parent = destinationParent
        destinationParent.children.append(sourceItem)

        return (accountString, .success)
    }

    public func download(
        remotePath: String,
        localPath: String,
        options: NKRequestOptions = .init(),
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
        remoteError: NKError
    ) {
        guard let item = item(remotePath: remotePath) else {
            return (accountString, nil, nil, 0, nil, nil, .urlError)
        }

        let localUrl = URL(fileURLWithPath: localPath)
        do {
            try item.data?.write(to: localUrl, options: .atomic)
        } catch let error {
            print("Could not write item data: \(error)")
            return (accountString, nil, nil, 0, nil, nil, .urlError)
        }

        return (
            accountString,
            item.versionIdentifier,
            item.creationDate as NSDate,
            item.size,
            nil,
            nil,
            .success
        )
    }

    public func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool = true,
        includeHiddenFiles: [String] = [],
        requestBody: Data? = nil,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, files: [NKFile], data: Data?, error: NKError) {
        guard let item = item(remotePath: remotePath) else {
            return (accountString, [], nil, .urlError)
        }

        switch depth {
        case .target:
            return (accountString, [item.nkfile], nil, .success)
        case .targetAndDirectChildren:
            return (accountString, [item.nkfile] + item.children.map { $0.nkfile }, nil, .success)
        case .targetAndAllChildren:
            var files = [NKFile]()
            var queue = [item]
            while !queue.isEmpty {
                var nextQueue = [MockRemoteItem]()
                for item in queue {
                    files.append(item.nkfile)
                    nextQueue.append(contentsOf: item.children)
                }
                queue = nextQueue
            }
            return (accountString, files, nil, .success)
        }
    }

    public func delete(
        remotePath: String,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, error: NKError) {
        guard let item = item(remotePath: remotePath) else {
            return (accountString, .urlError)
        }

        item.children = []
        item.parent?.children.removeAll(where: { $0.identifier == item.identifier })
        item.parent = nil
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
    ) async -> (account: String, data: Data?, error: NKError) {
        // TODO: Implement fetchCapabilities
        return (accountString, nil, .success)
    }
}
