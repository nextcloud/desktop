//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// Durable state of one multi-batch File Provider change enumeration.
struct ChangeDeliverySessionRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "changeDeliverySession"

    typealias Columns = CodingKeys

    enum CodingKeys: String, CodingKey, ColumnExpression {
        case sessionId, containerKey, currentAnchorKey, nextSequence, finalAnchorRawValue, incomplete, completed
        case pendingEndSequence, pendingAnchorKey, pendingMoreComing, pendingReported, hardRemoveDeleted
    }

    var sessionId: String
    var containerKey: String
    var currentAnchorKey: String
    var nextSequence = 0
    var finalAnchorRawValue: Data
    var incomplete: Bool
    var completed = false
    var pendingEndSequence = 0
    var pendingAnchorKey: String?
    var pendingMoreComing = false
    var pendingReported = false
    var hardRemoveDeleted: Bool

    /// The session as the tuple the change-delivery buffer consumes.
    var asTuple: (
        sessionId: String,
        containerKey: String,
        currentAnchorKey: String,
        nextSequence: Int,
        finalAnchorRawValue: Data,
        incomplete: Bool,
        pendingEndSequence: Int,
        pendingAnchorKey: String?,
        pendingMoreComing: Bool,
        pendingReported: Bool,
        hardRemoveDeleted: Bool
    ) {
        (
            sessionId,
            containerKey,
            currentAnchorKey,
            nextSequence,
            finalAnchorRawValue,
            incomplete,
            pendingEndSequence,
            pendingAnchorKey,
            pendingMoreComing,
            pendingReported,
            hardRemoveDeleted
        )
    }
}
