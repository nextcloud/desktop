//
//  Array+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-18.
//

extension Array {
    func chunked(into size: Int) -> [ArraySlice<Element>] {
        guard size > 0 else { return [] } // Avoid invalid chunk sizes
        return stride(from: 0, to: count, by: size).map { startIndex in
            self[startIndex..<Swift.min(startIndex + size, count)]
        }
    }

    func chunkedMap<T>(into size: Int, transform: (Element) -> T) -> [[T]] {
        return self.chunked(into: size).map { chunk in
            chunk.map(transform)
        }
    }

    func concurrentChunkedMap<T>(
        into size: Int,
        transform: @escaping (Element) -> T
    ) async -> [[T]] {
        await withTaskGroup(of: [T].self) { group in
            var results: [[T]] = []

            for chunk in chunked(into: size) {
                group.addTask {
                    return chunk.map { transform($0) }
                }
            }

            for await chunkResult in group {
                results.append(chunkResult)
            }

            return results
        }
    }
}
