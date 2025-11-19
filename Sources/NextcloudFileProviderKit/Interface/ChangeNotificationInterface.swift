//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

public protocol ChangeNotificationInterface: Sendable {
    func notifyChange()
}
