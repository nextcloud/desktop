/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
import Foundation

struct NextcloudAccount: Equatable {
    static let webDavFilesUrlSuffix: String = "/remote.php/dav/files/"
    let username, password, ncKitAccount, serverUrl, davFilesUrl: String

    init(user: String, serverUrl: String, password: String) {
        username = user
        self.password = password
        ncKitAccount = user + " " + serverUrl
        self.serverUrl = serverUrl
        davFilesUrl = serverUrl + NextcloudAccount.webDavFilesUrlSuffix + user
    }
}
