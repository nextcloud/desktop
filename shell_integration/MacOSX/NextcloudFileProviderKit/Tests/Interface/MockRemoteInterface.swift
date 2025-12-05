//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation
import NextcloudCapabilitiesKit
import NextcloudFileProviderKit
import NextcloudKit

let mockCapabilities = ##"""
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 100,
      "message": "OK",
      "totalitems": "",
      "itemsperpage": ""
    },
    "data": {
      "version": {
        "major": 28,
        "minor": 0,
        "micro": 4,
        "string": "28.0.4",
        "edition": "",
        "extendedSupport": false
      },
      "capabilities": {
        "core": {
          "pollinterval": 60,
          "webdav-root": "remote.php/webdav",
          "reference-api": true,
          "reference-regex": "(\\s|\\n|^)(https?:\\/\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\\n|$)"
        },
        "bruteforce": {
          "delay": 0,
          "allow-listed": false
        },
        "files": {
          "bigfilechunking": true,
          "blacklisted_files": [
            ".htaccess"
          ],
          "chunked_upload": {
            "max_size": 367001600,
            "max_parallel_count": 5
          },
          "directEditing": {
            "url": "https://mock.nc.com/ocs/v2.php/apps/files/api/v1/directEditing",
            "etag": "c748e8fc588b54fc5af38c4481a19d20",
            "supportsFileId": true
          },
          "locking": "1.0",
          "comments": true,
          "undelete": true,
          "versioning": true,
          "version_labeling": true,
          "version_deletion": true
        },
        "activity": {
          "apiv2": [
            "filters",
            "filters-api",
            "previews",
            "rich-strings"
          ]
        },
        "circles": {
          "version": "28.0.0",
          "status": {
            "globalScale": false
          },
          "settings": {
            "frontendEnabled": true,
            "allowedCircles": 262143,
            "allowedUserTypes": 31,
            "membersLimit": -1
          },
          "circle": {
            "constants": {
              "flags": {
                "1": "Single",
                "2": "Personal",
                "4": "System",
                "8": "Visible",
                "16": "Open",
                "32": "Invite",
                "64": "Join Request",
                "128": "Friends",
                "256": "Password Protected",
                "512": "No Owner",
                "1024": "Hidden",
                "2048": "Backend",
                "4096": "Local",
                "8192": "Root",
                "16384": "Circle Invite",
                "32768": "Federated",
                "65536": "Mount point"
              },
              "source": {
                "core": {
                  "1": "Nextcloud Account",
                  "2": "Nextcloud Group",
                  "4": "Email Address",
                  "8": "Contact",
                  "16": "Circle",
                  "10000": "Nextcloud App"
                },
                "extra": {
                  "10001": "Circles App",
                  "10002": "Admin Command Line"
                }
              }
            },
            "config": {
              "coreFlags": [
                1,
                2,
                4
              ],
              "systemFlags": [
                512,
                1024,
                2048
              ]
            }
          },
          "member": {
            "constants": {
              "level": {
                "1": "Member",
                "4": "Moderator",
                "8": "Admin",
                "9": "Owner"
              }
            },
            "type": {
              "0": "single",
              "1": "user",
              "2": "group",
              "4": "mail",
              "8": "contact",
              "16": "circle",
              "10000": "app"
            }
          }
        },
        "ocm": {
          "enabled": true,
          "apiVersion": "1.0-proposal1",
          "endPoint": "https://mock.nc.com/ocm",
          "resourceTypes": [
            {
              "name": "file",
              "shareTypes": [
                "user",
                "group"
              ],
              "protocols": {
                "webdav": "/public.php/webdav/"
              }
            }
          ]
        },
        "dav": {
          "chunking": "1.0",
          "bulkupload": "1.0"
        },
        "deck": {
          "version": "1.12.2",
          "canCreateBoards": true,
          "apiVersions": [
            "1.0",
            "1.1"
          ]
        },
        "files_sharing": {
          "api_enabled": true,
          "public": {
            "enabled": true,
            "password": {
              "enforced": false,
              "askForOptionalPassword": false
            },
            "expire_date": {
              "enabled": true,
              "days": 7,
              "enforced": true
            },
            "multiple_links": true,
            "expire_date_internal": {
              "enabled": false
            },
            "expire_date_remote": {
              "enabled": false
            },
            "send_mail": false,
            "upload": true,
            "upload_files_drop": true
          },
          "resharing": true,
          "user": {
            "send_mail": false,
            "expire_date": {
              "enabled": true
            }
          },
          "group_sharing": true,
          "group": {
            "enabled": true,
            "expire_date": {
              "enabled": true
            }
          },
          "default_permissions": 31,
          "federation": {
            "outgoing": true,
            "incoming": true,
            "expire_date": {
              "enabled": true
            },
            "expire_date_supported": {
              "enabled": true
            }
          },
          "sharee": {
            "query_lookup_default": false,
            "always_show_unique": true
          },
          "sharebymail": {
            "enabled": true,
            "send_password_by_mail": true,
            "upload_files_drop": {
              "enabled": true
            },
            "password": {
              "enabled": true,
              "enforced": false
            },
            "expire_date": {
              "enabled": true,
              "enforced": true
            }
          }
        },
        "fulltextsearch": {
          "remote": true,
          "providers": [
            {
              "id": "deck",
              "name": "Deck"
            },
            {
              "id": "files",
              "name": "Files"
            }
          ]
        },
        "notes": {
          "api_version": [
            "0.2",
            "1.3"
          ],
          "version": "4.9.4"
        },
        "notifications": {
          "ocs-endpoints": [
            "list",
            "get",
            "delete",
            "delete-all",
            "icons",
            "rich-strings",
            "action-web",
            "user-status",
            "exists"
          ],
          "push": [
            "devices",
            "object-data",
            "delete"
          ],
          "admin-notifications": [
            "ocs",
            "cli"
          ]
        },
        "notify_push": {
          "type": [
            "files",
            "activities",
            "notifications"
          ],
          "endpoints": {
            "websocket": "wss://mock.nc.com/push/ws",
            "pre_auth": "https://mock.nc.com/apps/notify_push/pre_auth"
          }
        },
        "password_policy": {
          "minLength": 10,
          "enforceNonCommonPassword": true,
          "enforceNumericCharacters": false,
          "enforceSpecialCharacters": false,
          "enforceUpperLowerCase": false,
          "api": {
            "generate": "https://mock.nc.com/ocs/v2.php/apps/password_policy/api/v1/generate",
            "validate": "https://mock.nc.com/ocs/v2.php/apps/password_policy/api/v1/validate"
          }
        },
        "provisioning_api": {
          "version": "1.18.0",
          "AccountPropertyScopesVersion": 2,
          "AccountPropertyScopesFederatedEnabled": true,
          "AccountPropertyScopesPublishedEnabled": true
        },
        "richdocuments": {
          "version": "8.3.4",
          "mimetypes": [
            "application/vnd.oasis.opendocument.text",
            "application/vnd.oasis.opendocument.spreadsheet",
            "application/vnd.oasis.opendocument.graphics",
            "application/vnd.oasis.opendocument.presentation",
            "application/vnd.oasis.opendocument.text-flat-xml",
            "application/vnd.oasis.opendocument.spreadsheet-flat-xml",
            "application/vnd.oasis.opendocument.graphics-flat-xml",
            "application/vnd.oasis.opendocument.presentation-flat-xml",
            "application/vnd.lotus-wordpro",
            "application/vnd.visio",
            "application/vnd.ms-visio.drawing",
            "application/vnd.wordperfect",
            "application/rtf",
            "text/rtf",
            "application/msonenote",
            "application/msword",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
            "application/vnd.ms-word.document.macroEnabled.12",
            "application/vnd.ms-word.template.macroEnabled.12",
            "application/vnd.ms-excel",
            "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
            "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
            "application/vnd.ms-excel.sheet.macroEnabled.12",
            "application/vnd.ms-excel.template.macroEnabled.12",
            "application/vnd.ms-excel.addin.macroEnabled.12",
            "application/vnd.ms-excel.sheet.binary.macroEnabled.12",
            "application/vnd.ms-powerpoint",
            "application/vnd.openxmlformats-officedocument.presentationml.presentation",
            "application/vnd.openxmlformats-officedocument.presentationml.template",
            "application/vnd.openxmlformats-officedocument.presentationml.slideshow",
            "application/vnd.ms-powerpoint.addin.macroEnabled.12",
            "application/vnd.ms-powerpoint.presentation.macroEnabled.12",
            "application/vnd.ms-powerpoint.template.macroEnabled.12",
            "application/vnd.ms-powerpoint.slideshow.macroEnabled.12",
            "text/csv"
          ],
          "mimetypesNoDefaultOpen": [
            "image/svg+xml",
            "application/pdf",
            "text/plain",
            "text/spreadsheet"
          ],
          "mimetypesSecureView": [],
          "collabora": {
            "convert-to": {
              "available": true,
              "endpoint": "/cool/convert-to"
            },
            "hasMobileSupport": true,
            "hasProxyPrefix": false,
            "hasTemplateSaveAs": false,
            "hasTemplateSource": true,
            "hasWASMSupport": false,
            "hasZoteroSupport": true,
            "productName": "Collabora Online Development Edition",
            "productVersion": "23.05.10.1",
            "productVersionHash": "baa6eef",
            "serverId": "8bee4df3"
          },
          "direct_editing": true,
          "templates": true,
          "productName": "Nextcloud Office",
          "editonline_endpoint": "https://mock.nc.com/apps/richdocuments/editonline",
          "config": {
            "wopi_url": "https://mock.nc.com/",
            "public_wopi_url": "https://mock.nc.com",
            "wopi_callback_url": "",
            "disable_certificate_verification": null,
            "edit_groups": null,
            "use_groups": null,
            "doc_format": null,
            "timeout": 15
          }
        },
        "spreed": {
          "features": [
            "audio",
            "video",
            "chat-v2",
            "conversation-v4",
            "guest-signaling",
            "empty-group-room",
            "guest-display-names",
            "multi-room-users",
            "favorites",
            "last-room-activity",
            "no-ping",
            "system-messages",
            "delete-messages",
            "mention-flag",
            "in-call-flags",
            "conversation-call-flags",
            "notification-levels",
            "invite-groups-and-mails",
            "locked-one-to-one-rooms",
            "read-only-rooms",
            "listable-rooms",
            "chat-read-marker",
            "chat-unread",
            "webinary-lobby",
            "start-call-flag",
            "chat-replies",
            "circles-support",
            "force-mute",
            "sip-support",
            "sip-support-nopin",
            "chat-read-status",
            "phonebook-search",
            "raise-hand",
            "room-description",
            "rich-object-sharing",
            "temp-user-avatar-api",
            "geo-location-sharing",
            "voice-message-sharing",
            "signaling-v3",
            "publishing-permissions",
            "clear-history",
            "direct-mention-flag",
            "notification-calls",
            "conversation-permissions",
            "rich-object-list-media",
            "rich-object-delete",
            "unified-search",
            "chat-permission",
            "silent-send",
            "silent-call",
            "send-call-notification",
            "talk-polls",
            "breakout-rooms-v1",
            "recording-v1",
            "avatar",
            "chat-get-context",
            "single-conversation-status",
            "chat-keep-notifications",
            "typing-privacy",
            "remind-me-later",
            "bots-v1",
            "markdown-messages",
            "media-caption",
            "session-state",
            "note-to-self",
            "recording-consent",
            "sip-support-dialout",
            "message-expiration",
            "reactions",
            "chat-reference-id"
          ],
          "config": {
            "attachments": {
              "allowed": true,
              "folder": "/Talk"
            },
            "call": {
              "enabled": true,
              "breakout-rooms": true,
              "recording": false,
              "recording-consent": 0,
              "supported-reactions": [
                "❤️",
                "🎉",
                "👏",
                "👍",
                "👎",
                "😂",
                "🤩",
                "🤔",
                "😲",
                "😥"
              ],
              "sip-enabled": false,
              "sip-dialout-enabled": false,
              "predefined-backgrounds": [
                "1_office.jpg",
                "2_home.jpg",
                "3_abstract.jpg",
                "4_beach.jpg",
                "5_park.jpg",
                "6_theater.jpg",
                "7_library.jpg",
                "8_space_station.jpg"
              ],
              "can-upload-background": true,
              "can-enable-sip": true
            },
            "chat": {
              "max-length": 32000,
              "read-privacy": 0,
              "has-translation-providers": false,
              "typing-privacy": 0
            },
            "conversations": {
              "can-create": true
            },
            "previews": {
              "max-gif-size": 3145728
            },
            "signaling": {
              "session-ping-limit": 200,
              "hello-v2-token-key": "-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAECOu2NBMo4juGx6hHNIGa550gGaxN\nzqe/TPxsX3QRjCrkyvdQaltjuRt/9PddhpbMxcJSzwVLqZRVHylfllD8pg==\n-----END PUBLIC KEY-----\n"
            }
          },
          "version": "18.0.7"
        },
        "systemtags": {
          "enabled": true
        },
        "theming": {
          "name": "Nextcloud",
          "url": "https://nextcloud.com",
          "slogan": "a safe home for all your data",
          "color": "#6ea68f",
          "color-text": "#000000",
          "color-element": "#6ea68f",
          "color-element-bright": "#6ea68f",
          "color-element-dark": "#6ea68f",
          "logo": "https://mock.nc.com/core/img/logo/logo.svg?v=1",
          "background": "#6ea68f",
          "background-plain": true,
          "background-default": true,
          "logoheader": "https://mock.nc.com/core/img/logo/logo.svg?v=1",
          "favicon": "https://mock.nc.com/core/img/logo/logo.svg?v=1"
        },
        "user_status": {
          "enabled": true,
          "restore": true,
          "supports_emoji": true
        },
        "weather_status": {
          "enabled": true
        }
      }
    }
  }
}
"""##

public class MockRemoteInterface: RemoteInterface, @unchecked Sendable {
    ///
    /// `RemoteInterface` makes it necessary to bypass its API to fully register a mocked account object.
    ///
    /// Use ``injectMock(_:)`` to add a new one.
    ///
    public var mockedAccounts = [String: Account]()

    public var capabilities = mockCapabilities
    public var rootItem: MockRemoteItem?
    public var delegate: (any NextcloudKitDelegate)?
    public var rootTrashItem: MockRemoteItem?
    public var currentChunks: [String: [RemoteFileChunk]] = [:]
    public var completedChunkTransferSize: [String: Int64] = [:]
    public var pagination: Bool
    public var expectedEnumerationPaginationTokens: [String: String] = [:]
    public var forceNextPageOnLastContentPage: Bool = false

    // Handler to track enumerate calls
    public var enumerateCallHandler: ((String, EnumerateDepth, Bool, [String], Data?, Account, NKRequestOptions, @escaping (URLSessionTask) -> Void) -> Void)?

    public init(
        account: Account,
        rootItem: MockRemoteItem? = nil,
        rootTrashItem: MockRemoteItem? = nil,
        pagination: Bool = false
    ) {
        mockedAccounts[account.ncKitAccount] = account
        self.rootItem = rootItem
        self.rootTrashItem = rootTrashItem
        self.pagination = pagination
    }

    ///
    /// Use this to register a mocked account object in ``mockedAccounts`` which otherwise cannot be passed through fully with `RemoteInterface`.
    ///
    public func injectMock(_ account: Account) {
        mockedAccounts[account.ncKitAccount] = account
    }

    func sanitisedPath(_ path: String, account: Account) -> String? {
        var sanitisedPath = path
        var filesPath: String
        if sanitisedPath.hasPrefix(account.davFilesUrl) {
            filesPath = account.davFilesUrl
        } else if sanitisedPath.hasPrefix(account.trashUrl) {
            filesPath = account.trashUrl
        } else {
            print("Invalid files path! Cannot create sanitised path for \(path)")
            return nil
        }

        if sanitisedPath.hasPrefix(filesPath) {
            // Keep the leading slash for root path
            let trimCount = filesPath.last == "/" ? filesPath.count - 1 : filesPath.count
            sanitisedPath = String(sanitisedPath.dropFirst(trimCount))
        } else {
            print("WARNING: Unexpected files path! \(filesPath)")
        }

        if sanitisedPath != "/", sanitisedPath.last == "/" {
            sanitisedPath = String(sanitisedPath.dropLast())
        }
        if sanitisedPath.isEmpty {
            sanitisedPath = "/"
        }
        return sanitisedPath
    }

    func item(remotePath: String, account: String) -> MockRemoteItem? {
        guard let rootItem, !remotePath.isEmpty else {
            print("Invalid root item or remote path, cannot get item in item tree.")
            return nil
        }

        guard let account = mockedAccounts[account] else {
            return nil
        }

        let sanitisedPath = sanitisedPath(remotePath, account: account)

        guard sanitisedPath != "/" else {
            return remotePath.hasPrefix(account.trashUrl) ? rootTrashItem : rootItem
        }

        var pathComponents = sanitisedPath?.components(separatedBy: "/")
        if pathComponents?.first?.isEmpty == true { pathComponents?.removeFirst() }
        var currentNode = remotePath.hasPrefix(account.trashUrl) ? rootTrashItem : rootItem

        while pathComponents?.isEmpty == false {
            let component = pathComponents?.removeFirst()

            guard component?.isEmpty == false, let nextNode = currentNode?.children.first(where: { $0.name == component }) else {
                return nil
            }

            guard pathComponents?.isEmpty == false else {
                return nextNode // This is the target
            }

            currentNode = nextNode
        }

        return nil
    }

    func parentPath(path: String, account: Account) -> String {
        let sanitisedPath = sanitisedPath(path, account: account)
        var pathComponents = sanitisedPath?.components(separatedBy: "/")
        if pathComponents?.first?.isEmpty == true { pathComponents?.removeFirst() }
        guard pathComponents?.isEmpty == false else { return "/" }
        pathComponents?.removeLast()
        let rootPath = path.hasPrefix(account.trashUrl) ? account.trashUrl : account.davFilesUrl
        return rootPath + "/" + (pathComponents?.joined(separator: "/") ?? "")
    }

    func parentItem(path: String, account: Account) -> MockRemoteItem? {
        let parentRemotePath = parentPath(path: path, account: account)
        return item(remotePath: parentRemotePath, account: account.ncKitAccount)
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
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in }
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
        options _: NKRequestOptions = .init(),
        requestHandler _: @escaping (Alamofire.UploadRequest) -> Void = { _ in },
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in },
        progressHandler _: @escaping (Progress) -> Void = { _ in }
    ) async -> (
        account: String,
        ocId: String?,
        etag: String?,
        date: NSDate?,
        size: Int64,
        response: HTTPURLResponse?,
        remoteError: NKError
    ) {
        var itemName: String
        do {
            itemName = try name(from: remotePath)
            debugPrint("Handling item upload:", itemName)
        } catch {
            return (account.ncKitAccount, nil, nil, nil, 0, nil, .urlError)
        }

        let itemLocalUrl = URL(fileURLWithPath: localPath)
        var itemData: Data
        do {
            itemData = try Data(contentsOf: itemLocalUrl)
            debugPrint("Acquired data:", itemData)
        } catch {
            return (account.ncKitAccount, nil, nil, nil, 0, nil, .urlError)
        }

        guard let parent = parentItem(path: remotePath, account: account) else {
            return (account.ncKitAccount, nil, nil, nil, 0, nil, .urlError)
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
            .success
        )
    }

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
        currentNumChunksUpdateHandler _: @escaping (Int) -> Void = { _ in },
        chunkCounter _: @escaping (Int) -> Void = { _ in },
        log _: any FileProviderLogging,
        chunkUploadStartHandler: @escaping ([RemoteFileChunk]) -> Void = { _ in },
        requestHandler: @escaping (UploadRequest) -> Void = { _ in },
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
        progressHandler: @escaping (Progress) -> Void = { _ in },
        chunkUploadCompleteHandler: @escaping (RemoteFileChunk) -> Void = { _ in }
    ) async -> (
        account: String,
        fileChunks: [RemoteFileChunk]?,
        file: NKFile?,
        nkError: NKError
    ) {
        guard let remoteUrl = URL(string: remotePath) else {
            print("Invalid remote path!")
            return ("", nil, nil, .urlError)
        }

        // Create temp directory for file and create chunks within it
        let fm = FileManager.default
        let tempDirectoryUrl = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try! fm.createDirectory(atPath: tempDirectoryUrl.path, withIntermediateDirectories: true)

        // Access local file and gather metadata
        let fileSize = try! fm.attributesOfItem(atPath: localPath)[.size] as! Int

        var remainingFileSize = fileSize
        let numChunks = Int(ceil(Double(fileSize) / Double(chunkSize)))
        let newChunks = !remainingChunks.isEmpty
            ? remainingChunks
            : (0 ..< numChunks).map { chunkIndex in
                defer { remainingFileSize -= chunkSize }
                return RemoteFileChunk(
                    fileName: String(chunkIndex + 1),
                    size: Int64(min(chunkSize, remainingFileSize)),
                    remoteChunkStoreFolderName: remoteChunkStoreFolderName
                )
            }
        let preexistingChunks = currentChunks[remoteChunkStoreFolderName] ?? []
        let totalChunks = preexistingChunks + newChunks
        currentChunks[remoteChunkStoreFolderName] = totalChunks
        chunkUploadStartHandler(newChunks)

        let (_, ocId, etag, date, size, _, remoteError) = await upload(
            remotePath: remotePath,
            localPath: localPath,
            creationDate: creationDate,
            modificationDate: modificationDate,
            account: account,
            options: options,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler
        )
        newChunks.forEach { chunkUploadCompleteHandler($0) }
        print(remainingChunks)
        completedChunkTransferSize[remoteChunkStoreFolderName] =
            remainingChunks.reduce(0) { $0 + $1.size }

        var file = NKFile()
        file.fileName = remoteUrl.lastPathComponent
        file.etag = etag ?? ""
        file.size = size
        file.path = remotePath
        file.ocId = ocId ?? ""
        file.serverUrl = remoteUrl.deletingLastPathComponent().absoluteString
        file.urlBase = account.serverUrl
        file.user = account.username
        file.userId = account.id
        file.account = account.ncKitAccount
        file.creationDate = creationDate ?? Date()
        file.date = date as? Date ?? Date()

        return (account.ncKitAccount, totalChunks, file, remoteError)
    }

    public func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool = false,
        account: Account,
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, data: Data?, error: NKError) {
        print("Moving \(remotePathSource) to \(remotePathDestination)")

        let isTrashed = remotePathSource.hasPrefix(account.trashUrl)
        let isTrashing = remotePathDestination.hasPrefix(account.trashUrl)
        let isRestoreFromTrash = remotePathDestination.hasPrefix(account.trashRestoreUrl)

        guard !isTrashed || isRestoreFromTrash || isTrashing else {
            print("Illegal moving of trash item into non-restore path \(remotePathDestination)")
            return (account.ncKitAccount, nil, .urlError)
        }

        guard !isRestoreFromTrash || overwrite else {
            print("Cannot restore from trash without overwriting!")
            return (account.ncKitAccount, nil, .urlError)
        }

        guard let sourceItem = item(remotePath: remotePathSource, account: account.ncKitAccount) else {
            print("Could not get item for remote path source\(remotePathSource)")
            return (account.ncKitAccount, nil, .urlError)
        }

        guard !isRestoreFromTrash || sourceItem.trashbinOriginalLocation != nil else {
            print("Cannot restore item from trash, sourceItem has no trashbin original location")
            return (account.ncKitAccount, nil, .urlError)
        }

        if isTrashing {
            sourceItem.identifier = sourceItem.identifier + trashedItemIdSuffix
        }

        sourceItem.name = try! name(from: remotePathDestination)
        sourceItem.parent?.children.removeAll(where: { $0.identifier == sourceItem.identifier })

        // Shadow remotePathDestination and set it to the item's original destination if it is a
        // restore from trash operation
        let remotePathDestination = isRestoreFromTrash && sourceItem.trashbinOriginalLocation != nil
            ? account.davFilesUrl + "/" + sourceItem.trashbinOriginalLocation!
            : remotePathDestination

        guard let destinationParent = parentItem(path: remotePathDestination, account: account) else {
            print("Failed to find destination parent item!")
            return (account.ncKitAccount, nil, .urlError)
        }

        if isRestoreFromTrash {
            sourceItem.identifier =
                sourceItem.identifier.replacingOccurrences(of: trashedItemIdSuffix, with: "")
            sourceItem.name = try! name(from: sourceItem.trashbinOriginalLocation!)
            sourceItem.trashbinOriginalLocation = nil
        }

        let matchingNameChildCount =
            destinationParent.children.count(where: { $0.name == sourceItem.name })

        if !overwrite, matchingNameChildCount > 0 {
            sourceItem.name += " (\(matchingNameChildCount))"
            print("Found conflicting children, renaming file to \(sourceItem.name)")
        } else if overwrite, matchingNameChildCount > 0 {
            print("Found conflicting children, removing all due to overwrite.")
            destinationParent.children.removeAll(where: { $0.name == sourceItem.name })
        }

        sourceItem.parent = destinationParent
        destinationParent.children.append(sourceItem)

        let oldPath = sourceItem.remotePath
        sourceItem.remotePath = destinationParent.remotePath + "/" + sourceItem.name

        print("Moved \(sourceItem.name) to \(remotePathDestination) (isTrashed: \(isTrashing))")

        var children = sourceItem.children

        while !children.isEmpty {
            var nextChildren = [MockRemoteItem]()
            for child in children {
                if isTrashing {
                    if child.remotePath.hasPrefix(account.davFilesUrl) {
                        child.trashbinOriginalLocation = child.remotePath.replacingOccurrences(
                            of: account.davFilesUrl + "/", with: ""
                        )
                        child.identifier = child.identifier + trashedItemIdSuffix
                    }
                } else {
                    child.identifier =
                        child.identifier.replacingOccurrences(of: trashedItemIdSuffix, with: "")
                    child.trashbinOriginalLocation = nil
                }

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

    public func downloadAsync(
        serverUrlFileName: Any,
        fileNameLocalPath: String,
        account: String,
        options _: NKRequestOptions,
        requestHandler _: @escaping (_ request: DownloadRequest) -> Void = { _ in },
        taskHandler _: @Sendable @escaping (_ task: URLSessionTask) -> Void = { _ in },
        progressHandler _: @escaping (_ progress: Progress) -> Void = { _ in }
    ) async -> (
        account: String,
        etag: String?,
        date: Date?,
        length: Int64,
        headers: [AnyHashable: any Sendable]?,
        afError: AFError?,
        nkError: NKError
    ) {
        guard let serverUrlFileName = serverUrlFileName as? String ?? (serverUrlFileName as? URL)?.absoluteString else {
            return (account, nil, nil, 0, nil, nil, .urlError)
        }

        guard let account = mockedAccounts[account] else {
            return (account, nil, nil, 0, nil, nil, .urlError)
        }

        guard let item = item(remotePath: serverUrlFileName, account: account.ncKitAccount) else {
            return (account.ncKitAccount, nil, nil, 0, nil, nil, .urlError)
        }

        let localUrl = URL(fileURLWithPath: fileNameLocalPath)

        do {
            if item.directory {
                print("Creating directory at \(localUrl) for item \(item.name)")
                let fm = FileManager.default
                try fm.createDirectory(at: localUrl, withIntermediateDirectories: true)
            } else {
                print("Writing data to \(localUrl) for item \(item.name)")
                try item.data?.write(to: localUrl)
            }
        } catch {
            print("Could not write item data: \(error)")
            return (account.ncKitAccount, nil, nil, 0, nil, nil, .urlError)
        }

        return (
            account.ncKitAccount,
            item.versionIdentifier,
            item.creationDate as Date,
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
    ) async -> (account: String, files: [NKFile], data: AFDataResponse<Data>?, error: NKError) {
        var remotePath = remotePath

        if remotePath.last == "." {
            remotePath.removeLast()
        }

        if remotePath.last == "/" {
            remotePath.removeLast()
        }

        print("Enumerating \(remotePath)")

        // Call the enumerate call handler if it exists
        enumerateCallHandler?(remotePath, depth, showHiddenFiles, includeHiddenFiles, requestBody, account, options, taskHandler)

        guard let item = item(remotePath: remotePath, account: account.ncKitAccount) else {
            print("Item at \(remotePath) not found.")
            return (
                account.ncKitAccount,
                [],
                nil,
                NKError(statusCode: 404, fallbackDescription: "File not found")
            )
        }

        func generateResponse(itemCount: Int, finalPage: Bool) -> AFDataResponse<Data>? {
            var responseHeaders: [String: String] = [:]
            if pagination, options.paginate {
                responseHeaders["X-NC-PAGINATE"] = "true"
                if options.paginateToken == nil {
                    responseHeaders["X-NC-PAGINATE-TOTAL"] = String(itemCount)
                }
                if finalPage {
                    expectedEnumerationPaginationTokens.removeValue(forKey: account.ncKitAccount)
                } else {
                    let token = UUID().uuidString
                    responseHeaders["X-NC-PAGINATE-TOKEN"] = token
                    expectedEnumerationPaginationTokens[account.ncKitAccount] = token
                }
            }

            return AFDataResponse<Data>(
                request: nil,
                response: HTTPURLResponse(
                    url: URL(string: account.davFilesUrl + remotePath)!,
                    statusCode: 200,
                    httpVersion: "HTTP/1.1",
                    headerFields: responseHeaders
                ),
                data: Data(),
                metrics: nil,
                serializationDuration: 0,
                result: .success(Data())
            )
        }

        let itemCount = options.paginateCount ?? .max
        let firstItem = options.paginateOffset ?? 0

        func generateReturn(files: [NKFile]) -> (
            account: String, files: [NKFile], data: AFDataResponse<Data>?, error: NKError
        ) {
            if pagination &&
                options.paginate &&
                options.paginateToken != expectedEnumerationPaginationTokens[account.ncKitAccount]
            {
                return (account.ncKitAccount, [], nil, .invalidData)
            }
            guard !forceNextPageOnLastContentPage || firstItem < files.count else {
                let responseData = generateResponse(itemCount: files.count, finalPage: true)
                return (account.ncKitAccount, [], responseData, .success)
            }
            let reachedEnd = firstItem + itemCount >= files.count
            let lastItem = min(firstItem + itemCount, files.count) - 1
            assert(firstItem <= lastItem)
            let itemsPage = Array(files[firstItem ... lastItem])
            let responseData = generateResponse(itemCount: files.count, finalPage: reachedEnd)
            return (account.ncKitAccount, itemsPage, responseData, .success)
        }

        switch depth {
            case .target:
                let responseData = generateResponse(itemCount: 1, finalPage: true)
                return (account.ncKitAccount, [item.toNKFile()], responseData, .success)
            case .targetAndDirectChildren:
                let files = [item.toNKFile()] + item.children.map { $0.toNKFile() }
                return generateReturn(files: files)
            case .targetAndAllChildren:
                var files = [NKFile]()
                var queue = [item]
                while !queue.isEmpty {
                    var nextQueue = [MockRemoteItem]()
                    for item in queue {
                        files.append(item.toNKFile())
                        nextQueue.append(contentsOf: item.children)
                    }
                    queue = nextQueue
                }
                return generateReturn(files: files)
        }
    }

    public func delete(
        remotePath: String,
        account: Account,
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, response: HTTPURLResponse?, error: NKError) {
        guard let item = item(remotePath: remotePath, account: account.ncKitAccount) else {
            return (account.ncKitAccount, nil, .urlError)
        }

        let relativePath =
            item.remotePath.replacingOccurrences(of: account.davFilesUrl + "/", with: "")
        item.trashbinOriginalLocation = relativePath

        let (_, _, error) = await move(
            remotePathSource: item.remotePath,
            remotePathDestination: account.trashUrl + "/" + item.name + " (trashed)",
            account: account
        )
        guard error == .success else { return (account.ncKitAccount, nil, error) }

        return (account.ncKitAccount, nil, .success)
    }

    public func lockUnlockFile(serverUrlFileName: String, type _: NKLockType?, shouldLock: Bool, account: Account, options _: NKRequestOptions, taskHandler _: @escaping (URLSessionTask) -> Void) async throws -> NKLock? {
        guard let item = item(remotePath: serverUrlFileName, account: account.ncKitAccount) else {
            throw NKError.urlError
        }

        item.locked = shouldLock

        return nil
    }

    public func listingTrashAsync(
        filename _: String?,
        showHiddenFiles _: Bool,
        account: String,
        options _: NKRequestOptions,
        taskHandler _: @Sendable @escaping (_ task: URLSessionTask) -> Void
    ) async -> (
        account: String,
        items: [NKTrash]?,
        responseData: AFDataResponse<Data>?,
        error: NKError
    ) {
        guard let rootTrashItem else {
            return (account, [], nil, .invalidData)
        }

        return (account, rootTrashItem.children.map { $0.toNKTrash() }, nil, .success)
    }

    public func restoreFromTrash(
        filename: String,
        account: Account,
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, data: Data?, error: NKError) {
        let fileTrashUrl = account.trashUrl + "/" + filename
        let fileTrashRestoreUrl = account.trashRestoreUrl + "/" + filename
        let (_, data, error) = await move(
            remotePathSource: fileTrashUrl,
            remotePathDestination: fileTrashRestoreUrl,
            overwrite: true,
            account: account
        )
        guard error == .success else {
            return (account.ncKitAccount, data, error)
        }
        return (account.ncKitAccount, data, .success)
    }

    public func downloadThumbnail(
        url _: URL,
        account: Account,
        options _: NKRequestOptions,
        taskHandler _: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        // TODO: Implement downloadThumbnail
        (account.ncKitAccount, nil, .success)
    }

    public func fetchCapabilities(
        account: Account,
        options _: NKRequestOptions,
        taskHandler _: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, capabilities: Capabilities?, data: Data?, error: NKError) {
        let capsData = capabilities.data(using: .utf8)
        return (account.ncKitAccount, directMockCapabilities(), capsData, .success)
    }

    public func directMockCapabilities() -> Capabilities? {
        let capsData = capabilities.data(using: .utf8)
        return Capabilities(data: capsData ?? Data())
    }

    public func getUserProfileAsync(
        account: String,
        options _: NKRequestOptions,
        taskHandler _: @Sendable @escaping (_ task: URLSessionTask) -> Void
    ) async -> (
        account: String,
        userProfile: NKUserProfile?,
        responseData: AFDataResponse<Data>?,
        error: NKError
    ) {
        guard let account = mockedAccounts[account] else {
            return (account, nil, nil, .urlError)
        }

        let profile = NKUserProfile()
        profile.address = account.serverUrl
        profile.backend = "mock"
        profile.displayName = account.ncKitAccount
        profile.userId = account.id

        return (account.ncKitAccount, profile, nil, .success)
    }

    public func tryAuthenticationAttempt(
        account: Account,
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> AuthenticationAttemptResultState {
        account.password.isEmpty ? .authenticationError : .success
    }
}
