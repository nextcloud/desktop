//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import RealmSwift
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks

fileprivate let mockCapabilities = ##"{"ocs":{"meta":{"status":"ok","statuscode":100,"message":"OK","totalitems":"","itemsperpage":""},"data":{"version":{"major":28,"minor":0,"micro":4,"string":"28.0.4","edition":"","extendedSupport":false},"capabilities":{"core":{"pollinterval":60,"webdav-root":"remote.php\/webdav","reference-api":true,"reference-regex":"(\\s|\\n|^)(https?:\\\/\\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\\n|$)"},"bruteforce":{"delay":0,"allow-listed":false},"files":{"bigfilechunking":true,"blacklisted_files":[".htaccess"],"directEditing":{"url":"localhost\/ocs\/v2.php\/apps\/files\/api\/v1\/directEditing","etag":"c748e8fc588b54fc5af38c4481a19d20","supportsFileId":true},"comments":true,"undelete":true,"versioning":true,"version_labeling":true,"version_deletion":true},"activity":{"apiv2":["filters","filters-api","previews","rich-strings"]},"circles":{"version":"28.0.0","status":{"globalScale":false},"settings":{"frontendEnabled":true,"allowedCircles":262143,"allowedUserTypes":31,"membersLimit":-1},"circle":{"constants":{"flags":{"1":"Single","2":"Personal","4":"System","8":"Visible","16":"Open","32":"Invite","64":"Join Request","128":"Friends","256":"Password Protected","512":"No Owner","1024":"Hidden","2048":"Backend","4096":"Local","8192":"Root","16384":"Circle Invite","32768":"Federated","65536":"Mount point"},"source":{"core":{"1":"Nextcloud Account","2":"Nextcloud Group","4":"Email Address","8":"Contact","16":"Circle","10000":"Nextcloud App"},"extra":{"10001":"Circles App","10002":"Admin Command Line"}}},"config":{"coreFlags":[1,2,4],"systemFlags":[512,1024,2048]}},"member":{"constants":{"level":{"1":"Member","4":"Moderator","8":"Admin","9":"Owner"}},"type":{"0":"single","1":"user","2":"group","4":"mail","8":"contact","16":"circle","10000":"app"}}},"ocm":{"enabled":true,"apiVersion":"1.0-proposal1","endPoint":"localhost\/ocm","resourceTypes":[{"name":"file","shareTypes":["user","group"],"protocols":{"webdav":"\/public.php\/webdav\/"}}]},"dav":{"chunking":"1.0","bulkupload":"1.0"},"deck":{"version":"1.12.2","canCreateBoards":true,"apiVersions":["1.0","1.1"]},"files_sharing":{"api_enabled":true,"public":{"enabled":true,"password":{"enforced":false,"askForOptionalPassword":false},"expire_date":{"enabled":true,"days":7,"enforced":true},"multiple_links":true,"expire_date_internal":{"enabled":false},"expire_date_remote":{"enabled":false},"send_mail":false,"upload":true,"upload_files_drop":true},"resharing":true,"user":{"send_mail":false,"expire_date":{"enabled":true}},"group_sharing":true,"group":{"enabled":true,"expire_date":{"enabled":true}},"default_permissions":31,"federation":{"outgoing":true,"incoming":true,"expire_date":{"enabled":true},"expire_date_supported":{"enabled":true}},"sharee":{"query_lookup_default":false,"always_show_unique":true},"sharebymail":{"enabled":true,"send_password_by_mail":true,"upload_files_drop":{"enabled":true},"password":{"enabled":true,"enforced":false},"expire_date":{"enabled":true,"enforced":true}}},"fulltextsearch":{"remote":true,"providers":[{"id":"deck","name":"Deck"},{"id":"files","name":"Files"}]},"notes":{"api_version":["0.2","1.3"],"version":"4.9.4"},"notifications":{"ocs-endpoints":["list","get","delete","delete-all","icons","rich-strings","action-web","user-status","exists"],"push":["devices","object-data","delete"],"admin-notifications":["ocs","cli"]},"notify_push":{"type":["files","activities","notifications"],"endpoints":{"websocket":"ws:\/\/localhost:8888\/websocket","pre_auth":"localhost\/apps\/notify_push\/pre_auth"}},"password_policy":{"minLength":10,"enforceNonCommonPassword":true,"enforceNumericCharacters":false,"enforceSpecialCharacters":false,"enforceUpperLowerCase":false,"api":{"generate":"localhost\/ocs\/v2.php\/apps\/password_policy\/api\/v1\/generate","validate":"localhost\/ocs\/v2.php\/apps\/password_policy\/api\/v1\/validate"}},"provisioning_api":{"version":"1.18.0","AccountPropertyScopesVersion":2,"AccountPropertyScopesFederatedEnabled":true,"AccountPropertyScopesPublishedEnabled":true},"richdocuments":{"version":"8.3.4","mimetypes":["application\/vnd.oasis.opendocument.text","application\/vnd.oasis.opendocument.spreadsheet","application\/vnd.oasis.opendocument.graphics","application\/vnd.oasis.opendocument.presentation","application\/vnd.oasis.opendocument.text-flat-xml","application\/vnd.oasis.opendocument.spreadsheet-flat-xml","application\/vnd.oasis.opendocument.graphics-flat-xml","application\/vnd.oasis.opendocument.presentation-flat-xml","application\/vnd.lotus-wordpro","application\/vnd.visio","application\/vnd.ms-visio.drawing","application\/vnd.wordperfect","application\/rtf","text\/rtf","application\/msonenote","application\/msword","application\/vnd.openxmlformats-officedocument.wordprocessingml.document","application\/vnd.openxmlformats-officedocument.wordprocessingml.template","application\/vnd.ms-word.document.macroEnabled.12","application\/vnd.ms-word.template.macroEnabled.12","application\/vnd.ms-excel","application\/vnd.openxmlformats-officedocument.spreadsheetml.sheet","application\/vnd.openxmlformats-officedocument.spreadsheetml.template","application\/vnd.ms-excel.sheet.macroEnabled.12","application\/vnd.ms-excel.template.macroEnabled.12","application\/vnd.ms-excel.addin.macroEnabled.12","application\/vnd.ms-excel.sheet.binary.macroEnabled.12","application\/vnd.ms-powerpoint","application\/vnd.openxmlformats-officedocument.presentationml.presentation","application\/vnd.openxmlformats-officedocument.presentationml.template","application\/vnd.openxmlformats-officedocument.presentationml.slideshow","application\/vnd.ms-powerpoint.addin.macroEnabled.12","application\/vnd.ms-powerpoint.presentation.macroEnabled.12","application\/vnd.ms-powerpoint.template.macroEnabled.12","application\/vnd.ms-powerpoint.slideshow.macroEnabled.12","text\/csv"],"mimetypesNoDefaultOpen":["image\/svg+xml","application\/pdf","text\/plain","text\/spreadsheet"],"mimetypesSecureView":[],"collabora":{"convert-to":{"available":true,"endpoint":"\/cool\/convert-to"},"hasMobileSupport":true,"hasProxyPrefix":false,"hasTemplateSaveAs":false,"hasTemplateSource":true,"hasWASMSupport":false,"hasZoteroSupport":true,"productName":"Collabora Online Development Edition","productVersion":"23.05.10.1","productVersionHash":"baa6eef","serverId":"8bee4df3"},"direct_editing":true,"templates":true,"productName":"Nextcloud Office","editonline_endpoint":"localhost\/apps\/richdocuments\/editonline","config":{"wopi_url":"localhost\/","public_wopi_url":"localhost","wopi_callback_url":"","disable_certificate_verification":null,"edit_groups":null,"use_groups":null,"doc_format":null,"timeout":15}},"spreed":{"features":["audio","video","chat-v2","conversation-v4","guest-signaling","empty-group-room","guest-display-names","multi-room-users","favorites","last-room-activity","no-ping","system-messages","delete-messages","mention-flag","in-call-flags","conversation-call-flags","notification-levels","invite-groups-and-mails","locked-one-to-one-rooms","read-only-rooms","listable-rooms","chat-read-marker","chat-unread","webinary-lobby","start-call-flag","chat-replies","circles-support","force-mute","sip-support","sip-support-nopin","chat-read-status","phonebook-search","raise-hand","room-description","rich-object-sharing","temp-user-avatar-api","geo-location-sharing","voice-message-sharing","signaling-v3","publishing-permissions","clear-history","direct-mention-flag","notification-calls","conversation-permissions","rich-object-list-media","rich-object-delete","unified-search","chat-permission","silent-send","silent-call","send-call-notification","talk-polls","breakout-rooms-v1","recording-v1","avatar","chat-get-context","single-conversation-status","chat-keep-notifications","typing-privacy","remind-me-later","bots-v1","markdown-messages","media-caption","session-state","note-to-self","recording-consent","sip-support-dialout","message-expiration","reactions","chat-reference-id"],"config":{"attachments":{"allowed":true,"folder":"\/Talk"},"call":{"enabled":true,"breakout-rooms":true,"recording":false,"recording-consent":0,"supported-reactions":["\u2764\ufe0f","\ud83c\udf89","\ud83d\udc4f","\ud83d\udc4d","\ud83d\udc4e","\ud83d\ude02","\ud83e\udd29","\ud83e\udd14","\ud83d\ude32","\ud83d\ude25"],"sip-enabled":false,"sip-dialout-enabled":false,"predefined-backgrounds":["1_office.jpg","2_home.jpg","3_abstract.jpg","4_beach.jpg","5_park.jpg","6_theater.jpg","7_library.jpg","8_space_station.jpg"],"can-upload-background":true,"can-enable-sip":true},"chat":{"max-length":32000,"read-privacy":0,"has-translation-providers":false,"typing-privacy":0},"conversations":{"can-create":true},"previews":{"max-gif-size":3145728},"signaling":{"session-ping-limit":200,"hello-v2-token-key":"-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAECOu2NBMo4juGx6hHNIGa550gGaxN\nzqe\/TPxsX3QRjCrkyvdQaltjuRt\/9PddhpbMxcJSzwVLqZRVHylfllD8pg==\n-----END PUBLIC KEY-----\n"}},"version":"18.0.7"},"systemtags":{"enabled":true},"theming":{"name":"Nextcloud","url":"https:\/\/nextcloud.com","slogan":"a safe home for all your data","color":"#6ea68f","color-text":"#000000","color-element":"#6ea68f","color-element-bright":"#6ea68f","color-element-dark":"#6ea68f","logo":"localhost\/core\/img\/logo\/logo.svg?v=1","background":"#6ea68f","background-plain":true,"background-default":true,"logoheader":"localhost\/core\/img\/logo\/logo.svg?v=1","favicon":"localhost\/core\/img\/logo\/logo.svg?v=1"},"user_status":{"enabled":true,"restore":true,"supports_emoji":true},"weather_status":{"enabled":true}}}}}"##

fileprivate let username = "testUser"
fileprivate let userId = "testUserId"
fileprivate let serverUrl = "localhost"
fileprivate let password = "abcd"

@available(macOS 14.0, iOS 17.0, *)
final class RemoteChangeObserverTests: NextcloudFileProviderKitTestCase {
    static let timeout = 5_000 // tries
    static let account = Account(
        user: username, id: userId, serverUrl: serverUrl, password: password
    )
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
    static let notifyPushServer = MockNotifyPushServer(
        host: serverUrl,
        port: 8888,
        username: username,
        password: password,
        eventLoopGroup: .singleton
    )
    var remoteChangeObserver: RemoteChangeObserver?

    override func setUp() {
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        Task { try await Self.notifyPushServer.run() }
    }

    override func tearDown() async throws {
        remoteChangeObserver?.resetWebSocket()
        remoteChangeObserver = nil
        Self.notifyPushServer.reset()
    }

    /// Helper to wait for an expectation with a standard timeout.
    private func wait(for expectation: XCTestExpectation, description: String) async {
        let result = await XCTWaiter.fulfillment(of: [expectation], timeout: 5.0)
        if result != .completed {
            XCTFail("Timeout waiting for \(description)")
        }
    }

    func testAuthentication() async throws {
        let remoteInterface = MockRemoteInterface()
        remoteInterface.capabilities = mockCapabilities

        var authenticated = false

        NotificationCenter.default.addObserver(
            forName: NotifyPushAuthenticatedNotificationName, object: nil, queue: nil
        ) { _ in
            authenticated = true
        }

        remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: MockChangeNotificationInterface(),
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)
    }

    func testRetryAuthentication() async throws {
        Self.notifyPushServer.delay = 1_000_000

        var authenticated = false

        NotificationCenter.default.addObserver(
            forName: NotifyPushAuthenticatedNotificationName, object: nil, queue: nil
        ) { _ in
            authenticated = true
        }

        let incorrectAccount =
            Account(user: username, id: userId, serverUrl: serverUrl, password: "wrong!")
        let remoteInterface = MockRemoteInterface()
        remoteInterface.capabilities = mockCapabilities
        remoteChangeObserver = RemoteChangeObserver(
            account: incorrectAccount,
            remoteInterface: remoteInterface,
            changeNotificationInterface: MockChangeNotificationInterface(),
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let remoteChangeObserver = remoteChangeObserver!

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_001)
            if remoteChangeObserver.webSocketAuthenticationFailCount > 0 {
                break
            }
        }
        XCTAssertTrue(remoteChangeObserver.webSocketAuthenticationFailCount > 0)
        remoteChangeObserver.account = Self.account

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)
        remoteChangeObserver.resetWebSocket()
    }

    func testStopRetryingConnection() async throws {
        let incorrectAccount =
            Account(user: username, id: userId, serverUrl: serverUrl, password: "wrong!")
        let remoteInterface = MockRemoteInterface()
        remoteInterface.capabilities = mockCapabilities
        let remoteChangeObserver = RemoteChangeObserver(
            account: incorrectAccount,
            remoteInterface: remoteInterface,
            changeNotificationInterface: MockChangeNotificationInterface(),
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if remoteChangeObserver.webSocketAuthenticationFailCount ==
                remoteChangeObserver.webSocketAuthenticationFailLimit
            {
                break
            }
        }
        XCTAssertEqual(
            remoteChangeObserver.webSocketAuthenticationFailCount,
            remoteChangeObserver.webSocketAuthenticationFailLimit
        )
        XCTAssertFalse(remoteChangeObserver.webSocketTaskActive)
    }

    func testChangeRecognised() async throws {
        // 1. Arrange
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let testStartDate = Date()
        let remoteInterface =
            MockRemoteInterface(rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.capabilities = mockCapabilities

        // --- DB State (What the app thinks is true) ---
        // A materialised file in the root that will be updated.
        var rootFileToUpdate =
            SendableItemMetadata(ocId: "rootFile", fileName: "root-file.txt", account: Self.account)
        rootFileToUpdate.downloaded = true
        rootFileToUpdate.etag = "ETAG_OLD_ROOTFILE"
        Self.dbManager.addItemMetadata(rootFileToUpdate)

        // A materialised folder that will have its contents changed.
        var folderA =
            SendableItemMetadata(ocId: "folderA", fileName: "FolderA", account: Self.account)
        folderA.directory = true
        folderA.visitedDirectory = true
        folderA.etag = "ETAG_OLD_FOLDERA"
        Self.dbManager.addItemMetadata(folderA)

        // A materialised file inside FolderA that will be deleted.
        var fileInAToDelete =
            SendableItemMetadata(ocId: "fileInA", fileName: "file-in-a.txt", account: Self.account)
        fileInAToDelete.downloaded = true
        fileInAToDelete.serverUrl = Self.account.davFilesUrl + "/FolderA"
        // Set an explicit old sync time to verify it gets updated during deletion
        fileInAToDelete.syncTime = Date(timeIntervalSince1970: 1000)
        Self.dbManager.addItemMetadata(fileInAToDelete)

        // A materialised folder that will be deleted entirely.
        var folderBToDelete =
            SendableItemMetadata(ocId: "folderB", fileName: "FolderB", account: Self.account)
        folderBToDelete.directory = true
        folderBToDelete.visitedDirectory = true
        // Set an explicit old sync time to verify it gets updated during deletion
        folderBToDelete.syncTime = Date(timeIntervalSince1970: 2000)
        Self.dbManager.addItemMetadata(folderBToDelete)

        // Record original sync times to verify they are updated during deletion
        let originalFileInASyncTime = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "fileInA")).syncTime
        let originalFolderBSyncTime = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folderB")).syncTime

        // --- Server State (The new "remote truth") ---
        let rootItem = remoteInterface.rootItem!

        // Update the root file on the server.
        let serverRootFile = MockRemoteItem(
            identifier: "rootFile",
            versionIdentifier: "ETAG_NEW_ROOTFILE",
            name: "root-file.txt",
            remotePath: Self.account.davFilesUrl + "/root-file.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.davFilesUrl
        )
        rootItem.children.append(serverRootFile)

        // Update FolderA on the server and modify its contents.
        let serverFolderA = MockRemoteItem(
            identifier: "folderA",
            versionIdentifier: "ETAG_NEW_FOLDERA",
            name: "FolderA",
            remotePath: Self.account.davFilesUrl + "/FolderA",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.davFilesUrl
        )
        rootItem.children.append(serverFolderA)

        // Add a new file inside FolderA on the server.
        let newFileInA = MockRemoteItem(
            identifier: "newFileInA",
            name: "new-file.txt",
            remotePath: serverFolderA.remotePath + "/new-file.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: serverFolderA.remotePath
        )
        serverFolderA.children.append(newFileInA)

        let authExpectation =
            XCTNSNotificationExpectation(name: NotifyPushAuthenticatedNotificationName)
        let changeNotifiedExpectation = XCTestExpectation(description: "Change Notified")
        let notificationInterface = MockChangeNotificationInterface()

        notificationInterface.changeHandler = {
            changeNotifiedExpectation.fulfill()
        }

        remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: notificationInterface,
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        // 2. Act & Assert
        await wait(for: authExpectation, description: "authentication")

        Self.notifyPushServer.send(message: "notify_file")

        await wait(for: changeNotifiedExpectation, description: "change notification")

        // 3. Assert Database State
        // Check updated items
        let finalRootFile = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "rootFile"))
        XCTAssertEqual(finalRootFile.etag, "ETAG_NEW_ROOTFILE")
        XCTAssertTrue(finalRootFile.syncTime >= testStartDate)

        let finalFolderA = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folderA"))
        XCTAssertEqual(finalFolderA.etag, "ETAG_NEW_FOLDERA")
        XCTAssertTrue(finalFolderA.syncTime >= testStartDate)

        // Check new item
        let finalNewFile = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "newFileInA"))
        XCTAssertEqual(finalNewFile.fileName, "new-file.txt")
        XCTAssertNotNil(finalNewFile.syncTime)

        // Check deleted items
        let deletedFileInA = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "fileInA"))
        XCTAssertTrue(
            deletedFileInA.deleted, "File inside updated folder should be marked as deleted."
        )
        XCTAssertTrue(deletedFileInA.syncTime >= testStartDate, 
                     "Deleted file's sync time should be updated to current time")
        XCTAssertGreaterThan(deletedFileInA.syncTime, originalFileInASyncTime,
                            "Deleted file's sync time should be newer than original sync time")

        let deletedFolderB = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "folderB"))
        XCTAssertTrue(deletedFolderB.deleted, "The entire folder should be marked as deleted.")
        XCTAssertTrue(deletedFolderB.syncTime >= testStartDate,
                     "Deleted folder's sync time should be updated to current time")
        XCTAssertGreaterThan(deletedFolderB.syncTime, originalFolderBSyncTime,
                            "Deleted folder's sync time should be newer than original sync time")
    }

    func testIgnoreNonFileNotifications() async throws {
        let remoteInterface = MockRemoteInterface()
        remoteInterface.capabilities = mockCapabilities

        var authenticated = false
        var notified = false

        NotificationCenter.default.addObserver(
            forName: NotifyPushAuthenticatedNotificationName, object: nil, queue: nil
        ) { _ in
            authenticated = true
        }

        let notificationInterface = MockChangeNotificationInterface()
        notificationInterface.changeHandler = { notified = true }
        remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: notificationInterface,
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)

        Self.notifyPushServer.send(message: "random")
        Self.notifyPushServer.send(message: "notify_activity")
        Self.notifyPushServer.send(message: "notify_notification")
        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if notified {
                break
            }
        }
        XCTAssertFalse(notified)
    }

    func testPolling() async throws {
        // 1. Arrange
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let remoteInterface = MockRemoteInterface(rootItem: MockRemoteItem.rootItem(account: Self.account))
        // No capabilities -> will force polling.
        remoteInterface.capabilities = ""

        // DB State: A materialised file with an old ETag.
        var fileToUpdate = SendableItemMetadata(ocId: "item1", fileName: "file.txt", account: Self.account)
        fileToUpdate.downloaded = true
        fileToUpdate.etag = "ETAG_OLD"
        Self.dbManager.addItemMetadata(fileToUpdate)

        // Server State: The same file now has a new ETag.
        let serverItem = MockRemoteItem(identifier: "item1", versionIdentifier: "ETAG_NEW", name: "file.txt", remotePath: Self.account.davFilesUrl + "/file.txt", account: Self.account.ncKitAccount, username: Self.account.username, userId: Self.account.id, serverUrl: Self.account.serverUrl)
        remoteInterface.rootItem?.children = [serverItem]

        let changeNotifiedExpectation = XCTestExpectation(description: "Change Notified via Polling")
        let notificationInterface = MockChangeNotificationInterface()
        notificationInterface.changeHandler = { changeNotifiedExpectation.fulfill() }

        remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: notificationInterface,
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        // Set a very short poll interval for the test.
        remoteChangeObserver?.pollInterval = 0.5

        // 2. Act & Assert
        // The observer will fail to connect to websocket and start polling.
        // We just need to wait for the poll to fire and detect the change.
        await wait(for: changeNotifiedExpectation, description: "polling to trigger change")
        XCTAssertTrue(remoteChangeObserver?.pollingActive ?? false, "Polling should be active.")
    }

    func testRetryOnRemoteClose() async throws {
        let remoteInterface = MockRemoteInterface()
        remoteInterface.capabilities = mockCapabilities

        var authenticated = false

        NotificationCenter.default.addObserver(
            forName: NotifyPushAuthenticatedNotificationName, object: nil, queue: nil
        ) { _ in
            authenticated = true
        }

        remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: MockChangeNotificationInterface(),
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)
        authenticated = false

        Self.notifyPushServer.resetCredentialsState()
        Self.notifyPushServer.closeConnections()

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)
    }

    func testPinging() async throws {
        let remoteInterface = MockRemoteInterface()
        remoteInterface.capabilities = mockCapabilities

        var authenticated = false

        NotificationCenter.default.addObserver(
            forName: NotifyPushAuthenticatedNotificationName, object: nil, queue: nil
        ) { _ in
            authenticated = true
        }

        remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: MockChangeNotificationInterface(),
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        let pingIntervalNsecs = 500_000_000
        remoteChangeObserver?.webSocketPingIntervalNanoseconds = UInt64(pingIntervalNsecs)

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)

        let intendedPings = 3
        // Add a bit of buffer to the wait time
        let intendedPingsWait = (intendedPings + 1) * pingIntervalNsecs

        var pings = 0
        Self.notifyPushServer.pingHandler = {
            pings += 1
        }

        try await Task.sleep(nanoseconds: UInt64(intendedPingsWait))
        XCTAssertEqual(pings, intendedPings)
    }

    func testRetryOnConnectionLoss() async throws {
        // 1. Arrange
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let remoteInterface =
            MockRemoteInterface(rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.capabilities = mockCapabilities

        // Setup a change scenario
        var fileToUpdate =
            SendableItemMetadata(ocId: "item1", fileName: "file.txt", account: Self.account)
        fileToUpdate.downloaded = true
        fileToUpdate.etag = "ETAG_OLD"
        Self.dbManager.addItemMetadata(fileToUpdate)
        let serverItem = MockRemoteItem(
            identifier: "item1",
            versionIdentifier: "ETAG_NEW",
            name: "file.txt",
            remotePath: Self.account.davFilesUrl + "/file.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteInterface.rootItem?.children = [serverItem]

        let notificationInterface = MockChangeNotificationInterface()
        let remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: remoteInterface,
            changeNotificationInterface: notificationInterface,
            domain: nil,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        self.remoteChangeObserver = remoteChangeObserver

        // --- Phase 1: Test connection and change notification ---
        let authExpectation =
            XCTNSNotificationExpectation(name: NotifyPushAuthenticatedNotificationName)
        remoteChangeObserver.networkReachabilityObserver(.reachableEthernetOrWiFi)
        await wait(for: authExpectation, description: "initial authentication")

        let changeExpectation1 = XCTestExpectation(description: "First change notification")
        notificationInterface.changeHandler = { changeExpectation1.fulfill() }
        Self.notifyPushServer.send(message: "notify_file")
        await wait(for: changeExpectation1, description: "first change")

        // --- Phase 2: Test connection loss ---
        remoteChangeObserver.networkReachabilityObserver(.notReachable)
        // Give it a moment to process the disconnection
        try await Task.sleep(nanoseconds: 200_000_000)
        XCTAssertFalse(
            remoteChangeObserver.webSocketTaskActive,
            "Websocket should be inactive after connection loss."
        )
        Self.notifyPushServer.reset()

        // --- Phase 3: Test reconnection and change notification ---
        let reauthExpectation =
            XCTNSNotificationExpectation(name: NotifyPushAuthenticatedNotificationName)

        // Trigger the reconnection logic.
        remoteChangeObserver.networkReachabilityObserver(.reachableEthernetOrWiFi)

        // Now, wait for the expectation to be fulfilled.
        await wait(for: reauthExpectation, description: "re-authentication")
        XCTAssertTrue(
            remoteChangeObserver.webSocketTaskActive,
            "Websocket should be active again after reconnection."
        )

        let changeExpectation2 = XCTestExpectation(description: "Second change notification")
        notificationInterface.changeHandler = { changeExpectation2.fulfill() }
        Self.notifyPushServer.send(message: "notify_file")
        await wait(for: changeExpectation2, description: "second change")
    }
}
