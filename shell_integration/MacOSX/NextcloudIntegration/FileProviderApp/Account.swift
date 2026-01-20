//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation

final class Account: Identifiable, Sendable {
    let address: URL
    var domain: NSFileProviderDomain?
    let id: UUID
    let password: String
    let user: String

    var displayName: String {
        "\(user)@\(address.absoluteString)"
    }

    init(address: URL, domain: NSFileProviderDomain? = nil, password: String, user: String) {
        self.address = address
        self.domain = domain
        self.id = UUID()
        self.password = password
        self.user = user
    }
}
