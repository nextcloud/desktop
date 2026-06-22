//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

public enum ShareType: Int {
    case user = 0
    case group = 1
    case publicLink = 3
    case email = 4
    case federatedCloudShare = 6
    case team = 7
    case talkConversation = 10
}
