//
//  NKShare+Extensions.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import NextcloudKit

extension NKShare {
    enum ShareType: Int {
        case user = 0
        case group = 1
        case publicLink = 3
        case email = 4
        case federatedCloud = 6
        case circle = 7
        case talkConversation = 10
    }
}
