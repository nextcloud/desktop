//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import RealmSwift

/// Durable state for one multi-batch File Provider change enumeration.
final class RealmChangeDeliverySession: Object {
    @Persisted(primaryKey: true) var sessionId = ""
    @Persisted var currentAnchorKey = ""
    @Persisted var nextSequence = 0
    @Persisted var finalAnchorRawValue = Data()
    @Persisted var incomplete = false
    @Persisted var completed = false

    convenience init(
        sessionId: String,
        currentAnchorKey: String,
        finalAnchorRawValue: Data,
        incomplete: Bool
    ) {
        self.init()
        self.sessionId = sessionId
        self.currentAnchorKey = currentAnchorKey
        self.finalAnchorRawValue = finalAnchorRawValue
        self.incomplete = incomplete
    }
}
