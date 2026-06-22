//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudFileProviderKit

public final class MockChangeNotificationInterface: ChangeNotificationInterface {
    public typealias ChangeHandler = @Sendable () -> Void

    let changeHandler: ChangeHandler?

    public init(changeHandler: ChangeHandler? = nil) {
        self.changeHandler = changeHandler
    }

    public func notifyChange() {
        changeHandler?()
    }
}
