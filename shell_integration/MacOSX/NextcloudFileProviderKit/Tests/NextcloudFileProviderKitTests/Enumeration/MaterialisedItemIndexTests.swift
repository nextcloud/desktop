//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct MaterialisedItemIndexTests {
    static let base = DatabaseTestSuites.account.davFilesUrl
    static let decomposedCafe = "Cafe\u{301}"
    static let composedCafe = "Caf\u{E9}"

    static let items: [SendableItemMetadata] = [
        item("a", "a", in: base + "/x"),
        item("b", "b", in: base + "/x"),
        item("bc-file", "file", in: base + "/x/bc"),
        item("cafe", decomposedCafe, in: base + "/x"),
        item("cafe-file", "file", in: base + "/x/" + decomposedCafe),
        item("same-1", "same", in: base + "/x"),
        item("same-2", "same", in: base + "/x"),
        item("deep", "deep", in: base + "/y/sub"),
        item("double-slash", "file", in: base + "/z/"),
        item("composed", composedCafe, in: base + "/w"),
        item("composed-file", "file", in: base + "/w/" + composedCafe),
        root
    ]

    static let root: SendableItemMetadata = {
        var metadata = item(NSFileProviderItemIdentifier.rootContainer.rawValue, "", in: base)
        metadata.directory = true
        return metadata
    }()

    static let queryPaths = [
        base, base + "/x", base + "/x/a", base + "/x/b", base + "/x/b/", base + "/x/bc",
        base + "/x/" + composedCafe, base + "/x/" + decomposedCafe, base + "/x/same",
        base + "/y", base + "/y/sub", base + "/y/sub/deep", base + "/x/none",
        base + "/z", base + "/z/", base + "/X", base + "/w/" + decomposedCafe, base + "/w/" + composedCafe
    ]

    static func item(_ ocId: String, _ fileName: String, in serverUrl: String) -> SendableItemMetadata {
        DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName, serverUrl: serverUrl)
    }

    @Test func answersLikeThePathComparisonsOfTheItems() {
        let index = MaterialisedItemIndex(Self.items)

        for path in Self.queryPaths {
            #expect(index.hasDescendant(of: path) == Self.items.contains { $0.isDescendant(of: path) }, "\(path)")

            for ocId in Self.items.map(\.ocId) + ["absent"] {
                let expected = Self.items.contains { $0.ocId != ocId && ($0.hasSameRemotePath(as: path) || $0.isDescendant(of: path)) }
                #expect(index.hasContent(at: path, excluding: ocId) == expected, "\(path) excluding \(ocId)")
            }
        }

        for item in Self.items {
            #expect(index.item(ocId: item.ocId)?.ocId == item.ocId)
            #expect(index.hasDescendant(of: item.remotePath()) == Self.items.contains { $0.ocId != item.ocId && $0.isDescendant(of: item.remotePath()) })
        }

        #expect(index.item(ocId: "absent") == nil)
    }

    @Test func aSiblingWhoseNameExtendsAnotherIsNotBelowIt() {
        let index = MaterialisedItemIndex(Self.items)

        #expect(index.hasDescendant(of: Self.base + "/x/b") == false)
        #expect(index.hasDescendant(of: Self.base + "/x/bc"))
    }

    @Test func aComposedQueryFindsADecomposedRow() {
        let index = MaterialisedItemIndex(Self.items)

        #expect(index.hasContent(at: Self.base + "/x/" + Self.composedCafe, excluding: "cafe"))
    }

    @Test func theExcludedItemDoesNotCountBelowThePath() {
        let index = MaterialisedItemIndex(Self.items)

        #expect(index.hasContent(at: Self.base + "/y", excluding: "deep") == false)
        #expect(index.hasContent(at: Self.base + "/y", excluding: "absent"))
    }

    @Test func anotherItemAtTheSamePathCounts() {
        let index = MaterialisedItemIndex(Self.items)

        #expect(index.hasContent(at: Self.base + "/x/same", excluding: "same-1"))
        #expect(index.hasContent(at: Self.base + "/x/a", excluding: "a") == false)
    }
}
