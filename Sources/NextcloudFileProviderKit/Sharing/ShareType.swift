//
//  ShareType.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 28/4/25.
//

public enum ShareType: Int {
    case user = 0
    case group = 1
    case publicLink = 3
    case email = 4
    case federatedCloudShare = 6
    case team = 7
    case talkConversation = 10
}
