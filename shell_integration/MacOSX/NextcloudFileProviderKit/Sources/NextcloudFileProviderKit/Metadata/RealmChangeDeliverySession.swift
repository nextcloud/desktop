//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import RealmSwift

/// Durable state for one multi-batch File Provider change enumeration.
final class RealmChangeDeliverySession: Object {
    @Persisted(primaryKey: true) var sessionId = ""
    @Persisted var containerKey = ""
    @Persisted var currentAnchorKey = ""
    @Persisted var nextSequence = 0
    @Persisted var finalAnchorRawValue = Data()
    @Persisted var incomplete = false
    @Persisted var completed = false
    @Persisted var pendingEndSequence = 0
    @Persisted var pendingAnchorKey: String?
    @Persisted var pendingMoreComing = false
    @Persisted var pendingReported = false
    @Persisted var hardRemoveDeleted = false

    convenience init(
        sessionId: String,
        containerKey: String,
        currentAnchorKey: String,
        finalAnchorRawValue: Data,
        incomplete: Bool,
        hardRemoveDeleted: Bool
    ) {
        self.init()
        self.sessionId = sessionId
        self.containerKey = containerKey
        self.currentAnchorKey = currentAnchorKey
        self.finalAnchorRawValue = finalAnchorRawValue
        self.incomplete = incomplete
        self.hardRemoveDeleted = hardRemoveDeleted
    }
}
