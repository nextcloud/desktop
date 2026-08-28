//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import RealmSwift

/// One durably stored item in a multi-batch File Provider change enumeration.
final class RealmChangeDeliveryItem: Object {
    @Persisted(primaryKey: true) var primaryKey = ""
    @Persisted(indexed: true) var sessionId = ""
    @Persisted var sequence = 0
    @Persisted var metadataData = Data()
    @Persisted var deleted = false

    convenience init(
        sessionId: String,
        sequence: Int,
        metadataData: Data,
        deleted: Bool
    ) {
        self.init()
        primaryKey = "\(sessionId)|\(sequence)"
        self.sessionId = sessionId
        self.sequence = sequence
        self.metadataData = metadataData
        self.deleted = deleted
    }
}
