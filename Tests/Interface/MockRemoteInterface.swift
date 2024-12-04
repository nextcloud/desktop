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

fileprivate let mockCapabilities = ##"{"ocs":{"meta":{"status":"ok","statuscode":100,"message":"OK","totalitems":"","itemsperpage":""},"data":{"version":{"major":28,"minor":0,"micro":4,"string":"28.0.4","edition":"","extendedSupport":false},"capabilities":{"core":{"pollinterval":60,"webdav-root":"remote.php\/webdav","reference-api":true,"reference-regex":"(\\s|\\n|^)(https?:\\\/\\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\\n|$)"},"bruteforce":{"delay":0,"allow-listed":false},"files":{"bigfilechunking":true,"blacklisted_files":[".htaccess"],"directEditing":{"url":"https:\/\/mock.nc.com\/ocs\/v2.php\/apps\/files\/api\/v1\/directEditing","etag":"c748e8fc588b54fc5af38c4481a19d20","supportsFileId":true},"comments":true,"undelete":true,"versioning":true,"version_labeling":true,"version_deletion":true},"activity":{"apiv2":["filters","filters-api","previews","rich-strings"]},"circles":{"version":"28.0.0","status":{"globalScale":false},"settings":{"frontendEnabled":true,"allowedCircles":262143,"allowedUserTypes":31,"membersLimit":-1},"circle":{"constants":{"flags":{"1":"Single","2":"Personal","4":"System","8":"Visible","16":"Open","32":"Invite","64":"Join Request","128":"Friends","256":"Password Protected","512":"No Owner","1024":"Hidden","2048":"Backend","4096":"Local","8192":"Root","16384":"Circle Invite","32768":"Federated","65536":"Mount point"},"source":{"core":{"1":"Nextcloud Account","2":"Nextcloud Group","4":"Email Address","8":"Contact","16":"Circle","10000":"Nextcloud App"},"extra":{"10001":"Circles App","10002":"Admin Command Line"}}},"config":{"coreFlags":[1,2,4],"systemFlags":[512,1024,2048]}},"member":{"constants":{"level":{"1":"Member","4":"Moderator","8":"Admin","9":"Owner"}},"type":{"0":"single","1":"user","2":"group","4":"mail","8":"contact","16":"circle","10000":"app"}}},"ocm":{"enabled":true,"apiVersion":"1.0-proposal1","endPoint":"https:\/\/mock.nc.com\/ocm","resourceTypes":[{"name":"file","shareTypes":["user","group"],"protocols":{"webdav":"\/public.php\/webdav\/"}}]},"dav":{"chunking":"1.0","bulkupload":"1.0"},"deck":{"version":"1.12.2","canCreateBoards":true,"apiVersions":["1.0","1.1"]},"files_sharing":{"api_enabled":true,"public":{"enabled":true,"password":{"enforced":false,"askForOptionalPassword":false},"expire_date":{"enabled":true,"days":7,"enforced":true},"multiple_links":true,"expire_date_internal":{"enabled":false},"expire_date_remote":{"enabled":false},"send_mail":false,"upload":true,"upload_files_drop":true},"resharing":true,"user":{"send_mail":false,"expire_date":{"enabled":true}},"group_sharing":true,"group":{"enabled":true,"expire_date":{"enabled":true}},"default_permissions":31,"federation":{"outgoing":true,"incoming":true,"expire_date":{"enabled":true},"expire_date_supported":{"enabled":true}},"sharee":{"query_lookup_default":false,"always_show_unique":true},"sharebymail":{"enabled":true,"send_password_by_mail":true,"upload_files_drop":{"enabled":true},"password":{"enabled":true,"enforced":false},"expire_date":{"enabled":true,"enforced":true}}},"fulltextsearch":{"remote":true,"providers":[{"id":"deck","name":"Deck"},{"id":"files","name":"Files"}]},"notes":{"api_version":["0.2","1.3"],"version":"4.9.4"},"notifications":{"ocs-endpoints":["list","get","delete","delete-all","icons","rich-strings","action-web","user-status","exists"],"push":["devices","object-data","delete"],"admin-notifications":["ocs","cli"]},"notify_push":{"type":["files","activities","notifications"],"endpoints":{"websocket":"wss:\/\/mock.nc.com\/push\/ws","pre_auth":"https:\/\/mock.nc.com\/apps\/notify_push\/pre_auth"}},"password_policy":{"minLength":10,"enforceNonCommonPassword":true,"enforceNumericCharacters":false,"enforceSpecialCharacters":false,"enforceUpperLowerCase":false,"api":{"generate":"https:\/\/mock.nc.com\/ocs\/v2.php\/apps\/password_policy\/api\/v1\/generate","validate":"https:\/\/mock.nc.com\/ocs\/v2.php\/apps\/password_policy\/api\/v1\/validate"}},"provisioning_api":{"version":"1.18.0","AccountPropertyScopesVersion":2,"AccountPropertyScopesFederatedEnabled":true,"AccountPropertyScopesPublishedEnabled":true},"richdocuments":{"version":"8.3.4","mimetypes":["application\/vnd.oasis.opendocument.text","application\/vnd.oasis.opendocument.spreadsheet","application\/vnd.oasis.opendocument.graphics","application\/vnd.oasis.opendocument.presentation","application\/vnd.oasis.opendocument.text-flat-xml","application\/vnd.oasis.opendocument.spreadsheet-flat-xml","application\/vnd.oasis.opendocument.graphics-flat-xml","application\/vnd.oasis.opendocument.presentation-flat-xml","application\/vnd.lotus-wordpro","application\/vnd.visio","application\/vnd.ms-visio.drawing","application\/vnd.wordperfect","application\/rtf","text\/rtf","application\/msonenote","application\/msword","application\/vnd.openxmlformats-officedocument.wordprocessingml.document","application\/vnd.openxmlformats-officedocument.wordprocessingml.template","application\/vnd.ms-word.document.macroEnabled.12","application\/vnd.ms-word.template.macroEnabled.12","application\/vnd.ms-excel","application\/vnd.openxmlformats-officedocument.spreadsheetml.sheet","application\/vnd.openxmlformats-officedocument.spreadsheetml.template","application\/vnd.ms-excel.sheet.macroEnabled.12","application\/vnd.ms-excel.template.macroEnabled.12","application\/vnd.ms-excel.addin.macroEnabled.12","application\/vnd.ms-excel.sheet.binary.macroEnabled.12","application\/vnd.ms-powerpoint","application\/vnd.openxmlformats-officedocument.presentationml.presentation","application\/vnd.openxmlformats-officedocument.presentationml.template","application\/vnd.openxmlformats-officedocument.presentationml.slideshow","application\/vnd.ms-powerpoint.addin.macroEnabled.12","application\/vnd.ms-powerpoint.presentation.macroEnabled.12","application\/vnd.ms-powerpoint.template.macroEnabled.12","application\/vnd.ms-powerpoint.slideshow.macroEnabled.12","text\/csv"],"mimetypesNoDefaultOpen":["image\/svg+xml","application\/pdf","text\/plain","text\/spreadsheet"],"mimetypesSecureView":[],"collabora":{"convert-to":{"available":true,"endpoint":"\/cool\/convert-to"},"hasMobileSupport":true,"hasProxyPrefix":false,"hasTemplateSaveAs":false,"hasTemplateSource":true,"hasWASMSupport":false,"hasZoteroSupport":true,"productName":"Collabora Online Development Edition","productVersion":"23.05.10.1","productVersionHash":"baa6eef","serverId":"8bee4df3"},"direct_editing":true,"templates":true,"productName":"Nextcloud Office","editonline_endpoint":"https:\/\/mock.nc.com\/apps\/richdocuments\/editonline","config":{"wopi_url":"https:\/\/mock.nc.com\/","public_wopi_url":"https:\/\/mock.nc.com","wopi_callback_url":"","disable_certificate_verification":null,"edit_groups":null,"use_groups":null,"doc_format":null,"timeout":15}},"spreed":{"features":["audio","video","chat-v2","conversation-v4","guest-signaling","empty-group-room","guest-display-names","multi-room-users","favorites","last-room-activity","no-ping","system-messages","delete-messages","mention-flag","in-call-flags","conversation-call-flags","notification-levels","invite-groups-and-mails","locked-one-to-one-rooms","read-only-rooms","listable-rooms","chat-read-marker","chat-unread","webinary-lobby","start-call-flag","chat-replies","circles-support","force-mute","sip-support","sip-support-nopin","chat-read-status","phonebook-search","raise-hand","room-description","rich-object-sharing","temp-user-avatar-api","geo-location-sharing","voice-message-sharing","signaling-v3","publishing-permissions","clear-history","direct-mention-flag","notification-calls","conversation-permissions","rich-object-list-media","rich-object-delete","unified-search","chat-permission","silent-send","silent-call","send-call-notification","talk-polls","breakout-rooms-v1","recording-v1","avatar","chat-get-context","single-conversation-status","chat-keep-notifications","typing-privacy","remind-me-later","bots-v1","markdown-messages","media-caption","session-state","note-to-self","recording-consent","sip-support-dialout","message-expiration","reactions","chat-reference-id"],"config":{"attachments":{"allowed":true,"folder":"\/Talk"},"call":{"enabled":true,"breakout-rooms":true,"recording":false,"recording-consent":0,"supported-reactions":["\u2764\ufe0f","\ud83c\udf89","\ud83d\udc4f","\ud83d\udc4d","\ud83d\udc4e","\ud83d\ude02","\ud83e\udd29","\ud83e\udd14","\ud83d\ude32","\ud83d\ude25"],"sip-enabled":false,"sip-dialout-enabled":false,"predefined-backgrounds":["1_office.jpg","2_home.jpg","3_abstract.jpg","4_beach.jpg","5_park.jpg","6_theater.jpg","7_library.jpg","8_space_station.jpg"],"can-upload-background":true,"can-enable-sip":true},"chat":{"max-length":32000,"read-privacy":0,"has-translation-providers":false,"typing-privacy":0},"conversations":{"can-create":true},"previews":{"max-gif-size":3145728},"signaling":{"session-ping-limit":200,"hello-v2-token-key":"-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAECOu2NBMo4juGx6hHNIGa550gGaxN\nzqe\/TPxsX3QRjCrkyvdQaltjuRt\/9PddhpbMxcJSzwVLqZRVHylfllD8pg==\n-----END PUBLIC KEY-----\n"}},"version":"18.0.7"},"systemtags":{"enabled":true},"theming":{"name":"Nextcloud","url":"https:\/\/nextcloud.com","slogan":"a safe home for all your data","color":"#6ea68f","color-text":"#000000","color-element":"#6ea68f","color-element-bright":"#6ea68f","color-element-dark":"#6ea68f","logo":"https:\/\/mock.nc.com\/core\/img\/logo\/logo.svg?v=1","background":"#6ea68f","background-plain":true,"background-default":true,"logoheader":"https:\/\/mock.nc.com\/core\/img\/logo\/logo.svg?v=1","favicon":"https:\/\/mock.nc.com\/core\/img\/logo\/logo.svg?v=1"},"user_status":{"enabled":true,"restore":true,"supports_emoji":true},"weather_status":{"enabled":true}}}}}"##

public class MockRemoteInterface: RemoteInterface {
    public var capabilities = mockCapabilities
    public var rootItem: MockRemoteItem?
    public var delegate: (any NextcloudKitDelegate)?

    public init(rootItem: MockRemoteItem? = nil) {
        self.rootItem = rootItem
    }

    func sanitisedPath(_ path: String, account: Account) -> String {
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
        if sanitisedPath.isEmpty {
            sanitisedPath = "/"
        }
        return sanitisedPath
    }

    func item(remotePath: String, account: Account) -> MockRemoteItem? {
        guard let rootItem, !remotePath.isEmpty else { return nil }

        let sanitisedPath = sanitisedPath(remotePath, account: account)
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

    func parentPath(path: String, account: Account) -> String {
        let sanitisedPath = sanitisedPath(path, account: account)
        var pathComponents = sanitisedPath.components(separatedBy: "/")
        if pathComponents.first?.isEmpty == true { pathComponents.removeFirst() }
        guard !pathComponents.isEmpty else { return "/" }
        pathComponents.removeLast()
        return account.davFilesUrl + "/" + pathComponents.joined(separator: "/")
    }

    func parentItem(path: String, account: Account) -> MockRemoteItem? {
        let parentRemotePath = parentPath(path: path, account: account)
        return item(remotePath: parentRemotePath, account: account)
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

    public func setDelegate(_ delegate: any NextcloudKitDelegate) {
        self.delegate = delegate
    }

    public func createFolder(
        remotePath: String,
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError) {
        var itemName: String
        do {
            itemName = try name(from: remotePath)
        } catch {
            return (account.ncKitAccount, nil, nil, .urlError)
        }

        let item = MockRemoteItem(
            identifier: randomIdentifier(),
            name: itemName,
            remotePath: remotePath,
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        guard let parent = parentItem(path: remotePath, account: account) else {
            return (account.ncKitAccount, nil, nil, .urlError)
        }

        parent.children.append(item)
        item.parent = parent
        return (account.ncKitAccount, item.identifier, item.creationDate as NSDate, .success)
    }

    public func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date? = .init(),
        modificationDate: Date? = .init(),
        account: Account,
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
        response: HTTPURLResponse?,
        afError: AFError?,
        remoteError: NKError
    ) {
        var itemName: String
        do {
            itemName = try name(from: localPath)
            debugPrint("Handling item upload:", itemName)
        } catch {
            return (account.ncKitAccount, nil, nil, nil, 0, nil, nil, .urlError)
        }

        let itemLocalUrl = URL(fileURLWithPath: localPath)
        var itemData: Data
        do {
            itemData = try Data(contentsOf: itemLocalUrl)
            debugPrint("Acquired data:", itemData)
        } catch {
            return (account.ncKitAccount, nil, nil, nil, 0, nil, nil, .urlError)
        }

        guard let parent = parentItem(path: remotePath, account: account) else {
            return (account.ncKitAccount, nil, nil, nil, 0, nil, nil, .urlError)
        }
        debugPrint("Parent is:", parent.remotePath)

        var item: MockRemoteItem
        if let existingItem = parent.children.first(where: { $0.remotePath == remotePath }) {
            item = existingItem
            item.data = itemData
            item.modificationDate = modificationDate ?? .init()
            print("Updated item \(item.name)")
        } else {
            item = MockRemoteItem(
                identifier: randomIdentifier(),
                name: itemName,
                remotePath: remotePath,
                creationDate: creationDate ?? .init(),
                modificationDate: modificationDate ?? .init(),
                data: itemData,
                account: account.ncKitAccount,
                username: account.username,
                userId: account.id,
                serverUrl: account.serverUrl
            )

            parent.children.append(item)
            item.parent = parent
            print("Created item \(item.name)")
        }

        return (
            account.ncKitAccount,
            item.identifier,
            item.versionIdentifier,
            item.modificationDate as NSDate,
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
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, data: Data?, error: NKError) {
        guard let itemNewName = try? name(from: remotePathDestination),
              let sourceItem = item(remotePath: remotePathSource, account: account),
              let destinationParent = parentItem(path: remotePathDestination, account: account),
              (overwrite || !destinationParent.children.contains(where: { $0.name == itemNewName }))
        else { return (account.ncKitAccount, nil, .urlError) }

        sourceItem.name = itemNewName
        sourceItem.parent?.children.removeAll(where: { $0.identifier == sourceItem.identifier })
        sourceItem.parent = destinationParent
        destinationParent.children.append(sourceItem)

        let oldPath = sourceItem.remotePath
        sourceItem.remotePath = remotePathDestination

        print("Moved \(sourceItem.name) to \(remotePathDestination)")

        var children = sourceItem.children

        while !children.isEmpty {
            var nextChildren = [MockRemoteItem]()
            for child in children {
                let childNewPath =
                    child.remotePath.replacingOccurrences(of: oldPath, with: remotePathDestination)
                print("Updating child path \(child.remotePath) to \(childNewPath)")
                child.remotePath = childNewPath
                nextChildren.append(contentsOf: child.children)
            }
            children = nextChildren
        }

        return (account.ncKitAccount, nil, .success)
    }

    public func download(
        remotePath: String,
        localPath: String,
        account: Account,
        options: NKRequestOptions = .init(),
        requestHandler: @escaping (DownloadRequest) -> Void = { _ in },
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
        progressHandler: @escaping (Progress) -> Void = { _ in }
    ) async -> (
        account: String,
        etag: String?,
        date: NSDate?,
        length: Int64,
        response: HTTPURLResponse?,
        afError: AFError?,
        remoteError: NKError
    ) {
        guard let item = item(remotePath: remotePath, account: account) else {
            return (account.ncKitAccount, nil, nil, 0, nil, nil, .urlError)
        }

        let localUrl = URL(fileURLWithPath: localPath)
        do {
            if item.directory {
                print("Creating directory at \(localUrl) for item \(item.name)")
                let fm = FileManager.default
                try fm.createDirectory(at: localUrl, withIntermediateDirectories: true)
            } else {
                print("Writing data to \(localUrl) for item \(item.name)")
                try item.data?.write(to: localUrl, options: .atomic)
            }
        } catch let error {
            print("Could not write item data: \(error)")
            return (account.ncKitAccount, nil, nil, 0, nil, nil, .urlError)
        }

        return (
            account.ncKitAccount,
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
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, files: [NKFile], data: Data?, error: NKError) {
        guard let item = item(remotePath: remotePath, account: account) else {
            return (account.ncKitAccount, [], nil, .urlError)
        }

        switch depth {
        case .target:
            return (account.ncKitAccount, [item.nkfile], nil, .success)
        case .targetAndDirectChildren:
            return (account.ncKitAccount, [item.nkfile] + item.children.map { $0.nkfile }, nil, .success)
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
            return (account.ncKitAccount, files, nil, .success)
        }
    }

    public func delete(
        remotePath: String,
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, response: HTTPURLResponse?, error: NKError) {
        guard let item = item(remotePath: remotePath, account: account) else {
            return (account.ncKitAccount, nil, .urlError)
        }

        item.children = []
        item.parent?.children.removeAll(where: { $0.identifier == item.identifier })
        item.parent = nil

        return (account.ncKitAccount, nil, .success)
    }

    public func downloadThumbnail(
        url: URL,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        // TODO: Implement downloadThumbnail
        return (account.ncKitAccount, nil, .success)
    }

    public func fetchCapabilities(
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        return (account.ncKitAccount, capabilities.data(using: .utf8), .success)
    }

    public func fetchUserProfile(
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, userProfile: NKUserProfile?, data: Data?, error: NKError) {
        let profile = NKUserProfile()
        profile.address = account.serverUrl
        profile.backend = "mock"
        profile.displayName = account.ncKitAccount
        profile.userId = account.id
        return (account.ncKitAccount, profile, nil, .success)
    }

    public func tryAuthenticationAttempt(
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> AuthenticationAttemptResultState {
        return account.password.isEmpty ? .authenticationError : .success
    }
}
