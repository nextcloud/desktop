//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

///
/// Looks up a working-set scan's materialised items by ocId and by path.
///
struct MaterialisedItemIndex {
    private var itemsByOcId = [String: SendableItemMetadata]()
    private var ocIdsByPath = [String: Set<String>]()

    /// The number of items below each path, keyed by every prefix of an item's path that ends before a "/".
    private var descendantCounts = [String: Int]()

    init(_ items: some Sequence<SendableItemMetadata>) {
        for item in items where itemsByOcId[item.ocId] == nil {
            itemsByOcId[item.ocId] = item

            let path = Self.key(item.remotePath())
            ocIdsByPath[path, default: []].insert(item.ocId)

            var ancestor = Substring(path)
            while let separator = ancestor.lastIndex(of: "/") {
                ancestor = ancestor[..<separator]
                descendantCounts[String(ancestor), default: 0] += 1
            }
        }
    }

    func item(ocId: String) -> SendableItemMetadata? {
        itemsByOcId[ocId]
    }

    ///
    /// Whether an item other than `ocId` is at `path` or below it.
    ///
    func hasContent(at path: String, excluding ocId: String) -> Bool {
        let key = Self.key(path)
        let ownRowIsBelow = itemsByOcId[ocId]?.isDescendant(of: path) == true

        if (descendantCounts[key] ?? 0) - (ownRowIsBelow ? 1 : 0) > 0 {
            return true
        }

        return ocIdsByPath[key]?.contains { $0 != ocId } == true
    }

    ///
    /// Whether an item is below `path`.
    ///
    func hasDescendant(of path: String) -> Bool {
        descendantCounts[Self.key(path)] != nil
    }

    private static func key(_ path: String) -> String {
        path.precomposedStringWithCanonicalMapping
    }
}
