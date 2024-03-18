//
//  ShareCapabilities.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 13/3/24.
//

import Foundation
import OSLog

struct ShareCapabilities {
    struct EmailCapabilities {
        private(set) var passwordEnabled = false
        private(set) var passwordEnforced = false

        init(dict: [String: Any]) {
            Logger.shareCapabilities.debug("Parsing email capabilities: \(dict, privacy: .public)")

            if let passwordCapabilities = dict["password"] as? [String : Any] {
                passwordEnabled = passwordCapabilities["enabled"] as? Bool ?? false
                passwordEnforced = passwordCapabilities["enforced"] as? Bool ?? false
            }
        }
    }

    struct PublicLinkCapabilities {
        private(set) var enabled = false
        private(set) var allowUpload = false
        private(set) var supportsUploadOnly = false
        private(set) var askOptionalPassword = false
        private(set) var enforcePassword = false
        private(set) var enforceExpireDate = false
        private(set) var expireDateDays = 1
        private(set) var internalEnforceExpireDate = false
        private(set) var internalExpireDateDays = 1
        private(set) var remoteEnforceExpireDate = false
        private(set) var remoteExpireDateDays = 1
        private(set) var multipleAllowed = false

        init(dict: [String: Any]) {
            Logger.shareCapabilities.debug("Parsing link capabilities: \(dict, privacy: .public)")

            enabled = dict["enabled"] as? Bool ?? false
            allowUpload = dict["upload"] as? Bool ?? false
            supportsUploadOnly = dict["supports_upload_only"] as? Bool ?? false
            multipleAllowed = dict["multiple"] as? Bool ?? false

            if let passwordCaps = dict["password"] as? [String : Any] {
                askOptionalPassword = passwordCaps["askForOptionalPassword"] as? Bool ?? false
                enforcePassword = passwordCaps["enforced"] as? Bool ?? false
            }

            if let expireDateCapabilities = dict["expire_date"] as? [String: Any] {
                expireDateDays = expireDateCapabilities["days"] as? Int ?? 1
                enforceExpireDate = expireDateCapabilities["enforced"] as? Bool ?? false
            }

            if let internalExpDateCaps = dict["expire_date_internal"] as? [String: Any] {
                internalExpireDateDays = internalExpDateCaps["days"] as? Int ?? 1
                internalEnforceExpireDate = internalExpDateCaps["enforced"] as? Bool ?? false
            }

            if let remoteExpDateCaps = dict["expire_date_remote"] as? [String: Any] {
                remoteExpireDateDays = remoteExpDateCaps["days"] as? Int ?? 1
                remoteEnforceExpireDate = remoteExpDateCaps["enforced"] as? Bool ?? false
            }
        }
    }

    private(set) var apiEnabled = false
    private(set) var resharing = false
    private(set) var defaultPermissions = 0
    private(set) var email = EmailCapabilities(dict: [:])
    private(set) var publicLink = PublicLinkCapabilities(dict: [:])


    init() {
        Logger.shareCapabilities.warning("Providing defaulted share capabilities!")
    }

    init(json: Data) {
        guard let anyJson = try? JSONSerialization.jsonObject(with: json, options: []) else {
            let jsonString = String(data: json, encoding: .utf8) ?? "UNKNOWN"
            Logger.shareCapabilities.error(
                "Received capabilities is not valid JSON! \(jsonString, privacy: .public)"
            )
            return
        }

        guard let jsonDict = anyJson as? [String : Any],
              let ocsData = jsonDict["ocs"] as? [String : Any],
              let receivedData = ocsData["data"] as? [String : Any],
              let capabilities = receivedData["capabilities"] as? [String : Any],
              let sharingCapabilities = capabilities["files_sharing"] as? [String : Any]
        else {
            let jsonString = anyJson as? [String : Any] ?? ["UNKNOWN" : "UNKNOWN"]
            Logger.shareCapabilities.error(
                "Could not parse share capabilities! \(jsonString, privacy: .public)"
            )
            return
        }
        apiEnabled = sharingCapabilities["api_enabled"] as? Bool ?? false

        if let emailCapabilities = sharingCapabilities["sharebymail"] as? [String : Any] {
            email = EmailCapabilities(dict: emailCapabilities)
        }

        if let publicLinkCapabilities = sharingCapabilities["public"] as? [String : Any] {
            publicLink = PublicLinkCapabilities(dict: publicLinkCapabilities)
        }

        Logger.shareCapabilities.debug("Parses share capabilities.")
    }
}
