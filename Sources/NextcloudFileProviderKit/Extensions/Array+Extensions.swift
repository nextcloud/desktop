//
//  Array+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-18.
//

fileprivate let defaultChunkSize = 64

extension Array {
    func chunked(into size: Int = defaultChunkSize) -> [ArraySlice<Element>] {
        guard size > 0 else { return [] } // Avoid invalid chunk sizes
        return stride(from: 0, to: count, by: size).map { startIndex in
            self[startIndex..<Swift.min(startIndex + size, count)]
        }
    }

    func chunkedMap<T>(into size: Int = defaultChunkSize, transform: (Element) -> T) -> [[T]] {
        return self.chunked(into: size).map { chunk in
            chunk.map(transform)
        }
    }

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

    func concurrentChunkedCompactMap<T>(
        into size: Int = defaultChunkSize, transform: @escaping (Element) throws -> T?
    ) async throws -> [T] {
        try await withThrowingTaskGroup(of: [T].self) { group in
            var results = [T]()
            results.reserveCapacity(self.count)

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
