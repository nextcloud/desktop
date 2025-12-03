//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import XCTest

final class ChunkedArrayTests: NextcloudFileProviderKitTestCase {
    // MARK: - chunked(into:)

    func testChunkedEmptyArray() {
        let emptyArray: [Int] = []
        XCTAssertEqual(emptyArray.chunked(into: 3), [])
    }

    func testChunkedSingleElement() {
        let array = [1]
        XCTAssertEqual(array.chunked(into: 3), [[1]])
    }

    func testChunkedExactDivision() {
        let array = [1, 2, 3, 4, 5, 6]
        XCTAssertEqual(array.chunked(into: 2), [[1, 2], [3, 4], [5, 6]])
    }

    func testChunkedPartialDivision() {
        let array = [1, 2, 3, 4, 5]
        XCTAssertEqual(array.chunked(into: 2), [[1, 2], [3, 4], [5]])
    }

    func testChunkedInvalidSize() {
        let array = [1, 2, 3]
        XCTAssertEqual(array.chunked(into: 0), [])
        XCTAssertEqual(array.chunked(into: -1), [])
    }

    // MARK: - chunkedMap(into:transform:)

    func testChunkedMap() {
        let array = [1, 2, 3, 4]
        let transformed = array.chunkedMap(into: 2) { $0 * 2 }
        XCTAssertEqual(transformed, [[2, 4], [6, 8]])
    }

    func testChunkedMapEmptyArray() {
        let emptyArray: [Int] = []
        XCTAssertEqual(emptyArray.chunkedMap(into: 2) { $0 * 2 }, [])
    }

    func testChunkedMapInvalidSize() {
        let array = [1, 2, 3]
        XCTAssertEqual(array.chunkedMap(into: 0) { $0 * 2 }, [])
    }

    // MARK: - concurrentChunkedForEach(into:operation:)

    func testConcurrentChunkedForEach() async {
        actor ResultsCollector {
            var results = [Int]()

            func append(_ value: Int) {
                results.append(value)
            }

            func getResults() -> [Int] {
                results
            }
        }

        let array = [1, 2, 3, 4]
        let collector = ResultsCollector()

        await array.concurrentChunkedForEach(into: 2) { element in
            try? await Task.sleep(nanoseconds: 100_000_000) // Simulate work (100ms)
            await collector.append(element * 2)
        }
        let sortedResults = await collector.getResults().sorted()
        let expectedResults = [2, 4, 6, 8]
        XCTAssertEqual(sortedResults, expectedResults)
    }

    func testConcurrentChunkedForEachEmptyArray() async {
        actor ResultsCollector {
            var results = [Int]()

            func append(_ value: Int) {
                results.append(value)
            }

            func isEmpty() -> Bool {
                results.isEmpty
            }
        }

        let emptyArray: [Int] = []
        let collector = ResultsCollector()

        await emptyArray.concurrentChunkedForEach(into: 2) { element in
            await collector.append(element)
        }

        let isEmpty = await collector.isEmpty()
        XCTAssertTrue(isEmpty)
    }

    // MARK: - concurrentChunkedCompactMap(into:transform:)

    func testConcurrentChunkedCompactMap() async throws {
        let array = [1, 2, 3, 4, 5, 6]
        let results = try await array.concurrentChunkedCompactMap(into: 2) { $0 % 2 == 0 ? $0 : nil }
        XCTAssertEqual(results.sorted(), [2, 4, 6])
    }

    func testConcurrentChunkedCompactMapEmptyArray() async throws {
        let emptyArray: [Int] = []
        let results = try await emptyArray.concurrentChunkedCompactMap(into: 2) { $0 }
        XCTAssertTrue(results.isEmpty)
    }

    func testConcurrentChunkedCompactMapInvalidSize() async throws {
        let array = [1, 2, 3]
        let results = try await array.concurrentChunkedCompactMap(into: 0) { $0 }
        XCTAssertTrue(results.isEmpty)
    }
}
