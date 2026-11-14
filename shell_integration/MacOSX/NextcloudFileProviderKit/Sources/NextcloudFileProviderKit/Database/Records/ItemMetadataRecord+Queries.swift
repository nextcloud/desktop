//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Predicates over the `itemMetadata` table which every lookup composes.
///
/// Locations are compared on the normalized key columns, so callers pass raw strings and the normalization happens here, once. Dates are bound as the stored number, never as a `Date`, because a `Date` would be bound as text and never compare equal to a `REAL` column.
///
extension ItemMetadataRecord {
    typealias Columns = CodingKeys

    /// The row occupying a logical address.
    static func hasLocation(serverUrl: String, fileName: String) -> SQLExpression {
        Columns.normalizedFileName == fileName.precomposedStringWithCanonicalMapping
            && Columns.normalizedServerUrl == serverUrl.precomposedStringWithCanonicalMapping
    }

    /// Rows in a directory, optionally including everything beneath it.
    ///
    /// Descendants are matched with a half-open range over the indexed key: every key starting with `<directory>/` sorts between `<directory>/` and `<directory>0`, as `0` is the code point after `/`. The comparison is byte-wise, so it is case-sensitive and needs no pattern escaping.
    ///
    static func hasServerUrl(equalTo serverUrl: String, includingDescendants: Bool) -> SQLExpression {
        let canonicalServerUrl = serverUrl.precomposedStringWithCanonicalMapping

        guard includingDescendants else {
            return Columns.normalizedServerUrl == canonicalServerUrl
        }

        return Columns.normalizedServerUrl == canonicalServerUrl
            || (Columns.normalizedServerUrl >= canonicalServerUrl + "/" && Columns.normalizedServerUrl < canonicalServerUrl + "0")
    }

    /// Rows synchronized after the given moment.
    static func syncedAfter(_ date: Date) -> SQLExpression {
        Columns.syncTime > date.timeIntervalSinceReferenceDate
    }

    /// The `syncTime` of the row state last handed to the system. Not part of the record, see `FilesDatabaseManager.markReported(_:)`.
    static let reportedSyncTimeColumn = Column("reportedSyncTime")

    /// The remote state at which the working-set scan last reconciled a directory's materialized subtree. Not part of the record, see `FilesDatabaseManager.recordScannedRemoteStates(_:forgetting:)`.
    static let scannedRemoteStateColumn = Column("scannedRemoteState")

    /// Rows whose current state the system has not received yet.
    static var hasUnreportedState: SQLExpression {
        reportedSyncTimeColumn == nil || Columns.syncTime > reportedSyncTimeColumn
    }

    /// Rows which the file provider has materialized: visited directories and downloaded files.
    static var isMaterialised: SQLExpression {
        (Columns.directory == true && Columns.visitedDirectory == true)
            || (Columns.directory == false && Columns.downloaded == true)
    }

    /// Files whose download may be removed: downloaded, live and not pinned.
    static var isEvictableFile: SQLExpression {
        Columns.directory == false
            && Columns.downloaded == true
            && Columns.deleted == false
            && Columns.keepDownloaded == false
    }

    /// Rows whose raw file name ends with the suffix, compared byte-wise.
    static func fileNameEnds(with suffix: String) -> SQL {
        "substr(\(Columns.fileName), -length(\(suffix))) = \(suffix)"
    }
}
