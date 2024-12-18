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
}
