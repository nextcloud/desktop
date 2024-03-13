//
//  ShareCapabilities.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 13/3/24.
//

import Foundation

struct ShareCapabilities {
    private(set) var shareApiEnabled = false
    private(set) var shareEmailPasswordEnabled = false
    private(set) var shareEmailPasswordEnforced = false
    private(set) var sharePublicLinkEnabled = false
    private(set) var sharePublicLinkAllowUpload = false
    private(set) var sharePublicLinkSupportsUploadOnly = false
    private(set) var sharePublicLinkAskOptionalPassword = false
    private(set) var sharePublicLinkEnforcePassword = false
    private(set) var sharePublicLinkEnforceExpireDate = 0
    private(set) var sharePublicLinkExpireDateDays = false
    private(set) var shareInternalEnforceExpireDate = false
    private(set) var shareInternalExpireDateDays = 0
    private(set) var shareRemoteEnforceExpireDate = false
    private(set) var shareRemoteExpireDateDays = 0
    private(set) var sharePublicLinkMultiple = false
    private(set) var shareResharing = false
    private(set) var shareDefaultPermissions = 0
}
