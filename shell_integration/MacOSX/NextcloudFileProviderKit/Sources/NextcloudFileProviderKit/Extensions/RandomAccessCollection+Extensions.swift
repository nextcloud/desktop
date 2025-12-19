//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

private let defaultChunkSize = 64

extension RandomAccessCollection {
    /// Chunks a collection into an array of its subsequences.
    /// - Parameter size: The size of each chunk.
    /// - Returns: An array of subsequences (e.g., ArraySlice).
    func chunked(into size: Int = defaultChunkSize) -> [SubSequence] {
        guard size > 0 else { return [] } // Avoid invalid chunk sizes
        var chunks: [SubSequence] = []
        chunks.reserveCapacity(Int(count) / size + 1)

        var currentIndex = startIndex
        while currentIndex < endIndex {
            let nextIndex = index(currentIndex, offsetBy: size, limitedBy: endIndex) ?? endIndex
            chunks.append(self[currentIndex ..< nextIndex])
            currentIndex = nextIndex
        }
        return chunks
    }

    /// Chunks the collection and applies a transformation to each element.
    func chunkedMap<T>(into size: Int = defaultChunkSize, transform: (Element) -> T) -> [[T]] {
        chunked(into: size).map { chunk in
            chunk.map(transform)
        }
    }

    /// Performs an asynchronous `forEach` operation on the collection in concurrent chunks.
    func concurrentChunkedForEach(into size: Int = defaultChunkSize, operation: @escaping @Sendable (Element) async -> Void) async where Element: Sendable {
        await withTaskGroup(of: Void.self) { group in
            for chunk in chunked(into: size) {
                let chunkArray = Array(chunk)
                group.addTask(operation: { @Sendable in
                    for element in chunkArray {
                        await operation(element)
                    }
                })
            }
        }
    }

    /// Performs an asynchronous `compactMap` operation on the collection in concurrent chunks.
    func concurrentChunkedCompactMap<T>(into size: Int = defaultChunkSize, transform: @escaping @Sendable (Element) throws -> T?) async throws -> [T] where T: Sendable, Element: Sendable {
        try await withThrowingTaskGroup(of: [T].self) { group in
            var results = [T]()
            // Reserving capacity is still a good optimization, though we can't know the exact final count.
            results.reserveCapacity(Int(self.count))

            for chunk in chunked(into: size) {
                let chunkArray = Array(chunk) // Convert to Array to ensure Sendable

                group.addTask(operation: { @Sendable in
                    try chunkArray.compactMap {
                        try transform($0)
                    }
                })
            }

            for try await chunkResult in group {
                results += chunkResult
            }

            return results
        }
    }
}
