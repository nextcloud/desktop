//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

/// An item update or deletion saved in SQLite so the extension can resume sending changes to Finder.
struct ChangeDeliveryItemRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "changeDeliveryItem"

    typealias Columns = CodingKeys

    enum CodingKeys: String, CodingKey, ColumnExpression {
        case deleted
        case metadataData
        case sequence
        case sessionId
    }

    var sessionId: String
    var sequence: Int
    var metadataData: Data
    var deleted: Bool

    /// Write item metadata as JSON for the SQLite change queue.
    /// Save the Files or Trash root and the path below it in place of `serverUrl`.
    static func encodeMetadata(
        _ metadata: SendableItemMetadata,
        domainAccount: Account? = nil,
        using encoder: JSONEncoder = JSONEncoder()
    ) throws -> Data {
        let record = try ItemMetadataRecord(metadata, domainAccount: domainAccount)
        let data = try encoder.encode(record.metadata)
        let parent = record.storedDAVParent

        var fields = try snapshotFields(from: data)
        fields.removeValue(forKey: "serverUrl")
        fields["davRoot"] = parent.root.rawValue
        fields["parentDAVPath"] = parent.path
        return try JSONSerialization.data(withJSONObject: fields)
    }

    /// Decode a queued item's JSON into metadata with a full WebDAV parent URL in `serverUrl`.
    /// Use `davRoot` and `parentDAVPath` with the saved account details to build the URL.
    /// Older Realm snapshots already contain the full `serverUrl`.
    static func decodeMetadata(
        _ data: Data,
        using decoder: JSONDecoder = JSONDecoder()
    ) throws -> SendableItemMetadata {
        var fields = try snapshotFields(from: data)

        guard fields["davRoot"] != nil || fields["parentDAVPath"] != nil else {
            return try decoder.decode(SendableItemMetadata.self, from: data)
        }

        guard let davRoot = fields["davRoot"] as? String,
              let root = DAVRoot(rawValue: davRoot),
              let path = fields["parentDAVPath"] as? String,
              path.isEmpty || path.hasPrefix("/"),
              let baseURL = fields["urlBase"] as? String,
              let backendUserID = fields["userId"] as? String,
              !baseURL.isEmpty,
              !backendUserID.isEmpty
        else {
            throw DecodingError.dataCorrupted(.init(
                codingPath: [],
                debugDescription: "Could not build the parent URL for queued item metadata."
            ))
        }

        fields["serverUrl"] = Account.absoluteURL(
            for: DAVPath(root: root, path: path),
            accountBaseURL: baseURL,
            backendUserID: backendUserID
        )
        return try decoder.decode(SendableItemMetadata.self, from: JSONSerialization.data(withJSONObject: fields))
    }

    private static func snapshotFields(from data: Data) throws -> [String: Any] {
        guard let fields = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw DecodingError.dataCorrupted(.init(codingPath: [], debugDescription: "Expected a queued item metadata object."))
        }
        return fields
    }
}
