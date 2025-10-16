//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudFileProviderKit

public class MockChangeNotificationInterface: ChangeNotificationInterface {
    public var changeHandler: (() -> Void)?
    public init(changeHandler: (() -> Void)? = nil) {
        self.changeHandler = changeHandler
    }

    public func notifyChange() {
        changeHandler?()
    }
}
