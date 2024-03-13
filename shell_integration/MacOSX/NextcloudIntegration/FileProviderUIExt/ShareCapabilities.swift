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
    }

    struct PublicLinkCapabilities {
        private(set) var enabled = false
        private(set) var allowUpload = false
        private(set) var supportsUploadOnly = false
        private(set) var askOptionalPassword = false
        private(set) var enforcePassword = false
        private(set) var enforceExpireDate = false
        private(set) var expireDateDays = 0
        private(set) var multipleAllowed = false
    }

    struct InternalCapabilities {
        private(set) var enforceExpireDate = false
        private(set) var expireDateDays = 0
    }

    struct RemoteCapabilities {
        private(set) var enforceExpireDate = false
        private(set) var expireDateDays = 0
    }

    private(set) var apiEnabled = false
    private(set) var resharing = false
    private(set) var defaultPermissions = 0
    private(set) var email = EmailCapabilities()
    private(set) var publicLink = PublicLinkCapabilities()
    private(set) var internalShares = InternalCapabilities()
    private(set) var remote = RemoteCapabilities()
}
