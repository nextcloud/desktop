//
//  RandomAccessCollection+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-18.
//

import Foundation

fileprivate let defaultChunkSize = 64

extension RandomAccessCollection {
    /// Chunks a collection into an array of its subsequences.
    /// - Parameter size: The size of each chunk.
    /// - Returns: An array of subsequences (e.g., ArraySlice).
    func chunked(into size: Int = defaultChunkSize) -> [SubSequence] {
        guard size > 0 else { return [] } // Avoid invalid chunk sizes
        var chunks: [SubSequence] = []
        chunks.reserveCapacity(Int(self.count) / size + 1)

        var currentIndex = self.startIndex
        while currentIndex < self.endIndex {
            let nextIndex = self.index(currentIndex, offsetBy: size, limitedBy: self.endIndex) ?? self.endIndex
            chunks.append(self[currentIndex..<nextIndex])
            currentIndex = nextIndex
        }
        return chunks
    }

    /// Chunks the collection and applies a transformation to each element.
    func chunkedMap<T>(into size: Int = defaultChunkSize, transform: (Element) -> T) -> [[T]] {
        return self.chunked(into: size).map { chunk in
            chunk.map(transform)
        }
    }

    /// Performs an asynchronous `forEach` operation on the collection in concurrent chunks.
    func concurrentChunkedForEach(
        into size: Int = defaultChunkSize, operation: @escaping (Element) async -> Void
    ) async {
        await withTaskGroup(of: Void.self) { group in
            for chunk in chunked(into: size) {
                group.addTask {
                    for element in chunk {
                        await operation(element)
                    }
                }
            }
        }
    }

    /// Performs an asynchronous `compactMap` operation on the collection in concurrent chunks.
    func concurrentChunkedCompactMap<T>(
        into size: Int = defaultChunkSize, transform: @escaping (Element) throws -> T?
    ) async throws -> [T] {
        try await withThrowingTaskGroup(of: [T].self) { group in
            var results = [T]()
            // Reserving capacity is still a good optimization, though we can't know the exact final count.
            results.reserveCapacity(Int(self.count))

            for chunk in chunked(into: size) {
                group.addTask {
                    return try chunk.compactMap { try transform($0) }
                }
            }

            for try await chunkResult in group {
                results += chunkResult
            }

            return results
        }
    }
}
