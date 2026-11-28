// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing
import TestInterface

struct CreateUploadChecksumTests {
    private static let fileID = "00000027instanceid"
    private static let expectedChecksum = "SHA256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    private let metadata = SendableItemMetadata(
        ocId: CreateUploadChecksumTests.fileID,
        fileName: "document.txt",
        account: Account(user: "tester", id: "tester", serverUrl: "https://example.invalid", password: "")
    )

    @Test
    func matchesNestedChecksumWithSameFileID() {
        let body = response(checksum: Self.expectedChecksum)

        #expect(CreateUploadChecksum.matches(Self.expectedChecksum, metadata: metadata, responseBody: body))
    }

    @Test
    func ignoresMatchingChecksumForAnotherFileID() {
        let body = response(checksum: Self.expectedChecksum, fileID: "00000028instanceid")

        #expect(!CreateUploadChecksum.matches(Self.expectedChecksum, metadata: metadata, responseBody: body))
    }

    @Test
    func ignoresMatchingChecksumInFailedProperties() {
        let body = response(checksum: Self.expectedChecksum, status: "HTTP/1.1 404 Not Found")
        var metadata = metadata
        metadata.checksums = Self.expectedChecksum

        #expect(!CreateUploadChecksum.matches(Self.expectedChecksum, metadata: metadata, responseBody: body))
    }

    @Test(arguments: [
        "SHA256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA1:a9993e364706816aba3e25717850c26c9cd0d89d",
        nil
    ] as [String?])
    func ignoresDifferentWeakOrMissingChecksum(checksum: String?) {
        let body = response(checksum: checksum)

        #expect(!CreateUploadChecksum.matches(Self.expectedChecksum, metadata: metadata, responseBody: body))
    }

    private func response(checksum: String?, fileID: String = CreateUploadChecksumTests.fileID, status: String = "HTTP/1.1 200 OK") -> Data {
        let checksums = checksum.map { "<oc:checksums><oc:checksum>\($0)</oc:checksum></oc:checksums>" } ?? ""
        return Data("""
        <?xml version="1.0"?>
        <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
            <d:response>
                <d:href>/remote.php/dav/files/tester/document.txt</d:href>
                <d:propstat>
                    <d:prop>
                        <oc:id>\(fileID)</oc:id>
                        \(checksums)
                    </d:prop>
                    <d:status>\(status)</d:status>
                </d:propstat>
            </d:response>
        </d:multistatus>
        """.utf8)
    }
}
