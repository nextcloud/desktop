//
//  RemoteChangeObserverTests.swift
//
//
//  Created by Claudio Cambra on 16/5/24.
//

import Foundation
import NextcloudCapabilitiesKit
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

fileprivate let mockCapabilities = ##"{"ocs":{"meta":{"status":"ok","statuscode":100,"message":"OK","totalitems":"","itemsperpage":""},"data":{"version":{"major":28,"minor":0,"micro":4,"string":"28.0.4","edition":"","extendedSupport":false},"capabilities":{"core":{"pollinterval":60,"webdav-root":"remote.php\/webdav","reference-api":true,"reference-regex":"(\\s|\\n|^)(https?:\\\/\\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\\n|$)"},"bruteforce":{"delay":0,"allow-listed":false},"files":{"bigfilechunking":true,"blacklisted_files":[".htaccess"],"directEditing":{"url":"localhost\/ocs\/v2.php\/apps\/files\/api\/v1\/directEditing","etag":"c748e8fc588b54fc5af38c4481a19d20","supportsFileId":true},"comments":true,"undelete":true,"versioning":true,"version_labeling":true,"version_deletion":true},"activity":{"apiv2":["filters","filters-api","previews","rich-strings"]},"circles":{"version":"28.0.0","status":{"globalScale":false},"settings":{"frontendEnabled":true,"allowedCircles":262143,"allowedUserTypes":31,"membersLimit":-1},"circle":{"constants":{"flags":{"1":"Single","2":"Personal","4":"System","8":"Visible","16":"Open","32":"Invite","64":"Join Request","128":"Friends","256":"Password Protected","512":"No Owner","1024":"Hidden","2048":"Backend","4096":"Local","8192":"Root","16384":"Circle Invite","32768":"Federated","65536":"Mount point"},"source":{"core":{"1":"Nextcloud Account","2":"Nextcloud Group","4":"Email Address","8":"Contact","16":"Circle","10000":"Nextcloud App"},"extra":{"10001":"Circles App","10002":"Admin Command Line"}}},"config":{"coreFlags":[1,2,4],"systemFlags":[512,1024,2048]}},"member":{"constants":{"level":{"1":"Member","4":"Moderator","8":"Admin","9":"Owner"}},"type":{"0":"single","1":"user","2":"group","4":"mail","8":"contact","16":"circle","10000":"app"}}},"ocm":{"enabled":true,"apiVersion":"1.0-proposal1","endPoint":"localhost\/ocm","resourceTypes":[{"name":"file","shareTypes":["user","group"],"protocols":{"webdav":"\/public.php\/webdav\/"}}]},"dav":{"chunking":"1.0","bulkupload":"1.0"},"deck":{"version":"1.12.2","canCreateBoards":true,"apiVersions":["1.0","1.1"]},"files_sharing":{"api_enabled":true,"public":{"enabled":true,"password":{"enforced":false,"askForOptionalPassword":false},"expire_date":{"enabled":true,"days":7,"enforced":true},"multiple_links":true,"expire_date_internal":{"enabled":false},"expire_date_remote":{"enabled":false},"send_mail":false,"upload":true,"upload_files_drop":true},"resharing":true,"user":{"send_mail":false,"expire_date":{"enabled":true}},"group_sharing":true,"group":{"enabled":true,"expire_date":{"enabled":true}},"default_permissions":31,"federation":{"outgoing":true,"incoming":true,"expire_date":{"enabled":true},"expire_date_supported":{"enabled":true}},"sharee":{"query_lookup_default":false,"always_show_unique":true},"sharebymail":{"enabled":true,"send_password_by_mail":true,"upload_files_drop":{"enabled":true},"password":{"enabled":true,"enforced":false},"expire_date":{"enabled":true,"enforced":true}}},"fulltextsearch":{"remote":true,"providers":[{"id":"deck","name":"Deck"},{"id":"files","name":"Files"}]},"notes":{"api_version":["0.2","1.3"],"version":"4.9.4"},"notifications":{"ocs-endpoints":["list","get","delete","delete-all","icons","rich-strings","action-web","user-status","exists"],"push":["devices","object-data","delete"],"admin-notifications":["ocs","cli"]},"notify_push":{"type":["files","activities","notifications"],"endpoints":{"websocket":"ws:\/\/localhost:8888\/websocket","pre_auth":"localhost\/apps\/notify_push\/pre_auth"}},"password_policy":{"minLength":10,"enforceNonCommonPassword":true,"enforceNumericCharacters":false,"enforceSpecialCharacters":false,"enforceUpperLowerCase":false,"api":{"generate":"localhost\/ocs\/v2.php\/apps\/password_policy\/api\/v1\/generate","validate":"localhost\/ocs\/v2.php\/apps\/password_policy\/api\/v1\/validate"}},"provisioning_api":{"version":"1.18.0","AccountPropertyScopesVersion":2,"AccountPropertyScopesFederatedEnabled":true,"AccountPropertyScopesPublishedEnabled":true},"richdocuments":{"version":"8.3.4","mimetypes":["application\/vnd.oasis.opendocument.text","application\/vnd.oasis.opendocument.spreadsheet","application\/vnd.oasis.opendocument.graphics","application\/vnd.oasis.opendocument.presentation","application\/vnd.oasis.opendocument.text-flat-xml","application\/vnd.oasis.opendocument.spreadsheet-flat-xml","application\/vnd.oasis.opendocument.graphics-flat-xml","application\/vnd.oasis.opendocument.presentation-flat-xml","application\/vnd.lotus-wordpro","application\/vnd.visio","application\/vnd.ms-visio.drawing","application\/vnd.wordperfect","application\/rtf","text\/rtf","application\/msonenote","application\/msword","application\/vnd.openxmlformats-officedocument.wordprocessingml.document","application\/vnd.openxmlformats-officedocument.wordprocessingml.template","application\/vnd.ms-word.document.macroEnabled.12","application\/vnd.ms-word.template.macroEnabled.12","application\/vnd.ms-excel","application\/vnd.openxmlformats-officedocument.spreadsheetml.sheet","application\/vnd.openxmlformats-officedocument.spreadsheetml.template","application\/vnd.ms-excel.sheet.macroEnabled.12","application\/vnd.ms-excel.template.macroEnabled.12","application\/vnd.ms-excel.addin.macroEnabled.12","application\/vnd.ms-excel.sheet.binary.macroEnabled.12","application\/vnd.ms-powerpoint","application\/vnd.openxmlformats-officedocument.presentationml.presentation","application\/vnd.openxmlformats-officedocument.presentationml.template","application\/vnd.openxmlformats-officedocument.presentationml.slideshow","application\/vnd.ms-powerpoint.addin.macroEnabled.12","application\/vnd.ms-powerpoint.presentation.macroEnabled.12","application\/vnd.ms-powerpoint.template.macroEnabled.12","application\/vnd.ms-powerpoint.slideshow.macroEnabled.12","text\/csv"],"mimetypesNoDefaultOpen":["image\/svg+xml","application\/pdf","text\/plain","text\/spreadsheet"],"mimetypesSecureView":[],"collabora":{"convert-to":{"available":true,"endpoint":"\/cool\/convert-to"},"hasMobileSupport":true,"hasProxyPrefix":false,"hasTemplateSaveAs":false,"hasTemplateSource":true,"hasWASMSupport":false,"hasZoteroSupport":true,"productName":"Collabora Online Development Edition","productVersion":"23.05.10.1","productVersionHash":"baa6eef","serverId":"8bee4df3"},"direct_editing":true,"templates":true,"productName":"Nextcloud Office","editonline_endpoint":"localhost\/apps\/richdocuments\/editonline","config":{"wopi_url":"localhost\/","public_wopi_url":"localhost","wopi_callback_url":"","disable_certificate_verification":null,"edit_groups":null,"use_groups":null,"doc_format":null,"timeout":15}},"spreed":{"features":["audio","video","chat-v2","conversation-v4","guest-signaling","empty-group-room","guest-display-names","multi-room-users","favorites","last-room-activity","no-ping","system-messages","delete-messages","mention-flag","in-call-flags","conversation-call-flags","notification-levels","invite-groups-and-mails","locked-one-to-one-rooms","read-only-rooms","listable-rooms","chat-read-marker","chat-unread","webinary-lobby","start-call-flag","chat-replies","circles-support","force-mute","sip-support","sip-support-nopin","chat-read-status","phonebook-search","raise-hand","room-description","rich-object-sharing","temp-user-avatar-api","geo-location-sharing","voice-message-sharing","signaling-v3","publishing-permissions","clear-history","direct-mention-flag","notification-calls","conversation-permissions","rich-object-list-media","rich-object-delete","unified-search","chat-permission","silent-send","silent-call","send-call-notification","talk-polls","breakout-rooms-v1","recording-v1","avatar","chat-get-context","single-conversation-status","chat-keep-notifications","typing-privacy","remind-me-later","bots-v1","markdown-messages","media-caption","session-state","note-to-self","recording-consent","sip-support-dialout","message-expiration","reactions","chat-reference-id"],"config":{"attachments":{"allowed":true,"folder":"\/Talk"},"call":{"enabled":true,"breakout-rooms":true,"recording":false,"recording-consent":0,"supported-reactions":["\u2764\ufe0f","\ud83c\udf89","\ud83d\udc4f","\ud83d\udc4d","\ud83d\udc4e","\ud83d\ude02","\ud83e\udd29","\ud83e\udd14","\ud83d\ude32","\ud83d\ude25"],"sip-enabled":false,"sip-dialout-enabled":false,"predefined-backgrounds":["1_office.jpg","2_home.jpg","3_abstract.jpg","4_beach.jpg","5_park.jpg","6_theater.jpg","7_library.jpg","8_space_station.jpg"],"can-upload-background":true,"can-enable-sip":true},"chat":{"max-length":32000,"read-privacy":0,"has-translation-providers":false,"typing-privacy":0},"conversations":{"can-create":true},"previews":{"max-gif-size":3145728},"signaling":{"session-ping-limit":200,"hello-v2-token-key":"-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAECOu2NBMo4juGx6hHNIGa550gGaxN\nzqe\/TPxsX3QRjCrkyvdQaltjuRt\/9PddhpbMxcJSzwVLqZRVHylfllD8pg==\n-----END PUBLIC KEY-----\n"}},"version":"18.0.7"},"systemtags":{"enabled":true},"theming":{"name":"Nextcloud","url":"https:\/\/nextcloud.com","slogan":"a safe home for all your data","color":"#6ea68f","color-text":"#000000","color-element":"#6ea68f","color-element-bright":"#6ea68f","color-element-dark":"#6ea68f","logo":"localhost\/core\/img\/logo\/logo.svg?v=1","background":"#6ea68f","background-plain":true,"background-default":true,"logoheader":"localhost\/core\/img\/logo\/logo.svg?v=1","favicon":"localhost\/core\/img\/logo\/logo.svg?v=1"},"user_status":{"enabled":true,"restore":true,"supports_emoji":true},"weather_status":{"enabled":true}}}}}"##

@available(macOS 14.0, iOS 17.0, *)
final class RemoteChangeObserverTests: XCTestCase {
    static let timeout = 5_000 // tries
    static let account = Account(
        user: "testUser", serverUrl: "localhost", password: "abcd"
    )
    var remoteInterface: MockRemoteInterface!
    var notifyPushServer: MockNotifyPushServer!

    override func setUp() {
        super.setUp()
        remoteInterface = MockRemoteInterface(account: Self.account)
        remoteInterface.capabilities = mockCapabilities
        notifyPushServer = MockNotifyPushServer(
            host: Self.account.serverUrl, 
            port: 8888,
            username: Self.account.username,
            password: Self.account.password,
            eventLoopGroup: .singleton
        )
        Task { try await notifyPushServer.run() }
    }

    func testAuthentication() async throws {
        var authenticated = false

        NotificationCenter.default.addObserver(
            forName: NotifyPushAuthenticatedNotificationName, object: nil, queue: nil
        ) { _ in
            authenticated = true
        }
        
        let changeNotificationIntf = MockChangeNotificationInterface()
        let remoteChangeObserver = RemoteChangeObserver(
            remoteInterface: remoteInterface,
            changeNotificationInterface: changeNotificationIntf,
            domain: nil
        )

        for _ in 0...Self.timeout {
            try await Task.sleep(nanoseconds: 1_000_000)
            if authenticated {
                break
            }
        }
        XCTAssertTrue(authenticated)
    }

    // Test retry on unauthenticated
    // Test stop trying
    // Test retry on connection failure
    // Test retry on connection closed
    // Test retry on connection timeout
    // Test polling
}
