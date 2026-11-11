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
    /// Save a parent like `files:/Team` in `parentDAVPath` in place of the full `serverUrl`
    /// when the parent URL matches the account's Files or Trash root.
    static func encodeMetadata(
        _ metadata: SendableItemMetadata,
        domainAccount: Account? = nil,
        using encoder: JSONEncoder = JSONEncoder()
    ) throws -> Data {
        let record = ItemMetadataRecord(metadata, domainAccount: domainAccount)
        let data = try encoder.encode(record.metadata)
        guard let parentDAVPath = record.storedParentPath else { return data }

        var fields = try snapshotFields(from: data)
        fields.removeValue(forKey: "serverUrl")
        fields["parentDAVPath"] = parentDAVPath
        return try JSONSerialization.data(withJSONObject: fields)
    }

    /// Decode a queued item's JSON into metadata with a full WebDAV parent URL in `serverUrl`.
    /// For a saved path like `files:/Team`, use `urlBase` and `userId` from the same JSON object
    /// to rebuild `https://cloud.example.com/remote.php/dav/files/alice/Team`.
    static func decodeMetadata(
        _ data: Data,
        using decoder: JSONDecoder = JSONDecoder()
    ) throws -> SendableItemMetadata {
        var fields = try snapshotFields(from: data)

        guard let parentDAVPath = fields["parentDAVPath"] as? String else {
            return try decoder.decode(SendableItemMetadata.self, from: data)
        }

        guard let baseURL = fields["urlBase"] as? String,
              let backendUserID = fields["userId"] as? String,
              !baseURL.isEmpty,
              !backendUserID.isEmpty,
              let parentURL = Account.absoluteURL(
                  fromDAVRelativePath: parentDAVPath,
                  accountBaseURL: baseURL,
                  backendUserID: backendUserID
              )
        else {
            throw DecodingError.dataCorrupted(.init(codingPath: [], debugDescription: "Could not resolve the queued DAV parent path."))
        }

        fields["serverUrl"] = parentURL
        return try decoder.decode(SendableItemMetadata.self, from: JSONSerialization.data(withJSONObject: fields))
    }

    private static func snapshotFields(from data: Data) throws -> [String: Any] {
        guard let fields = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw DecodingError.dataCorrupted(.init(codingPath: [], debugDescription: "Expected a queued item metadata object."))
        }
        return fields
    }
}
