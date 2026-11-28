// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import CryptoKit
import Foundation

enum CreateUploadChecksum {
    private static let readBufferSize = 1024 * 1024

    static func header(forFileAt path: String, cancellation: NetworkOperationCancellation) throws -> String {
        let file = try FileHandle(forReadingFrom: URL(fileURLWithPath: path))
        defer { try? file.close() }

        var hash = SHA256()
        while let bytes = try file.read(upToCount: readBufferSize), !bytes.isEmpty {
            try cancellation.checkCancellation()
            try Task.checkCancellation()
            hash.update(data: bytes)
        }
        let digest = hash.finalize().map { String(format: "%02x", $0) }.joined()
        return "SHA256:\(digest)"
    }

    static func matches(_ expected: String, metadata: SendableItemMetadata, responseBody: Data?) -> Bool {
        let canonicalChecksum = expected.lowercased()
        guard let responseBody else {
            return metadata.checksums.split(whereSeparator: \.isWhitespace)
                .contains { $0.lowercased() == canonicalChecksum }
        }

        // NextcloudKit currently reads the checksum container's text and misses its child elements.
        let dav = "DAV:"
        let ownCloud = "http://owncloud.org/ns"
        guard !responseBody.isEmpty,
              let document = try? XMLDocument(data: responseBody, options: [.nodeLoadExternalEntitiesNever]),
              let responses = document.rootElement()?.elements(forLocalName: "response", uri: dav),
              responses.count == 1,
              let response = responses.first
        else {
            return false
        }

        let properties = response.elements(forLocalName: "propstat", uri: dav).compactMap { propstat -> XMLElement? in
            let status = propstat.elements(forLocalName: "status", uri: dav).first?.stringValue
            guard status?.split(separator: " ").dropFirst().first == "200" else { return nil }
            return propstat.elements(forLocalName: "prop", uri: dav).first
        }
        guard properties.contains(where: { $0.elements(forLocalName: "id", uri: ownCloud).first?.stringValue == metadata.ocId }) else {
            return false
        }

        return properties
            .flatMap { $0.elements(forLocalName: "checksums", uri: ownCloud) }
            .flatMap { $0.elements(forLocalName: "checksum", uri: ownCloud) }
            .contains { $0.stringValue?.trimmingCharacters(in: .whitespacesAndNewlines).lowercased() == canonicalChecksum }
    }
}
