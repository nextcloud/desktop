//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB

///
/// Query expressions for the `itemMetadata` table.
///
/// Locations are compared on the normalized key columns, so callers pass stored locations and normalization happens here. Dates are bound as the stored number, never as a `Date`, because a `Date` would be bound as text and never compare equal to a `REAL` column.
///
extension ItemMetadataRecord {
    typealias Columns = CodingKeys

    /// A root row shares the same parent URL as top-level items.
    static var isFilesRoot: SQLExpression {
        Columns.ocId == NSFileProviderItemIdentifier.rootContainer.rawValue
            || (Columns.directory == true && Columns.fileName == SendableItemMetadata.filesRootPlaceholderName)
    }

    /// Match the root, normalized parent path, and normalized filename together.
    static func matchesStoredParentLocationAndFileName(
        davRoot: DAVRoot,
        parentPath: String,
        fileName: String
    ) -> SQLExpression {
        Columns.davRoot == davRoot.rawValue
            && Columns.normalizedParentPath == parentPath.precomposedStringWithCanonicalMapping
            && Columns.normalizedFileName == fileName.precomposedStringWithCanonicalMapping
    }

    /// Rows in a directory, optionally including everything beneath it.
    ///
    /// Descendants are matched with a half-open range over the indexed key: every key starting with `<directory>/` sorts between `<directory>/` and `<directory>0`, as `0` is the code point after `/`. The comparison is byte-wise, so it is case-sensitive and needs no pattern escaping.
    ///
    static func matchesStoredParentLocation(davRoot: DAVRoot, parentPath: String, includingDescendants: Bool) -> SQLExpression {
        let normalizedParentPath = parentPath.precomposedStringWithCanonicalMapping
        let matchesRoot = Columns.davRoot == davRoot.rawValue
        let matchesParent = Columns.normalizedParentPath == normalizedParentPath

        guard includingDescendants else {
            return matchesRoot && matchesParent
        }

        // Bound the combined lookup so SQLite can search both the DAV root and path in the index.
        // Within those bounds, keep the directory itself and paths starting with its next slash.
        return matchesRoot
            && Columns.normalizedParentPath >= normalizedParentPath
            && Columns.normalizedParentPath < normalizedParentPath + "0"
            && (matchesParent || Columns.normalizedParentPath >= normalizedParentPath + "/")
    }

    /// Rows synchronized after the given moment.
    static func syncedAfter(_ date: Date) -> SQLExpression {
        Columns.syncTime > date.timeIntervalSinceReferenceDate
    }

    /// Rows which the file provider has materialized: visited directories and downloaded files.
    static var isMaterialised: SQLExpression {
        (Columns.directory == true && Columns.visitedDirectory == true)
            || (Columns.directory == false && Columns.downloaded == true)
    }

    static var hasUnfinishedUpload: SQLExpression {
        Columns.status == Status.inUpload.rawValue
            || Columns.status == Status.uploading.rawValue
            || (Columns.status == Status.uploadError.rawValue && !Columns.uploaded)
    }

    /// The transfers an account change can interrupt.
    static var hasActiveTransfer: SQLExpression {
        isDownloading || isUploading
    }

    /// Allow listings to refresh interrupted downloads and keep interrupted uploads pending.
    /// Clear the upload attempt ID so a result from the old attempt cannot complete it.
    static var interruptedTransferAssignments: [ColumnAssignment] {
        let status = SQL("""
            CASE
                WHEN \(isDownloading) THEN \(Status.normal.rawValue)
                WHEN \(isUploading) THEN \(Status.uploadError.rawValue)
                ELSE \(Columns.status)
            END
            """).sqlExpression
        let session = SQL("CASE WHEN \(isUploading) THEN '' ELSE \(Columns.session) END").sqlExpression
        return [
            Columns.status.set(to: status),
            Columns.session.set(to: session)
        ]
    }

    private static var isDownloading: SQLExpression {
        [Status.inDownload.rawValue, Status.downloading.rawValue].contains(Columns.status)
    }

    private static var isUploading: SQLExpression {
        Columns.status == Status.uploading.rawValue
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
