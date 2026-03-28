//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

@Suite("DirtyUserDataObserver Tests")
struct DirtyUserDataObserverTests {
    let mockLog: FileProviderLogMock

    init() {
        mockLog = FileProviderLogMock()
    }

    @Test("Observer initialization")
    func initialization() {
        var completionCalled = false

        _ = DirtyUserDataObserver(log: mockLog) { _ in
            completionCalled = true
        }

        #expect(!completionCalled, "Completion should not be called on initialization")
    }

    @Test("Finish enumerating with no items should report no dirty data")
    func finishEnumeratingWithNoItems() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == false, "Should report no dirty data when no items enumerated")
    }

    @Test("Enumerate only uploaded items should report no dirty data")
    func enumerateOnlyUploadedItems() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let uploadedItems = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("2"), filename: "file2.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("3"), filename: "file3.txt", isUploaded: true)
        ]

        observer.didEnumerate(uploadedItems)
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == false, "Should report no dirty data when all items are uploaded")
    }

    @Test("Enumerate with one non-uploaded item should report dirty data")
    func enumerateWithNonUploadedItem() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let mixedItems = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("2"), filename: "file2.txt", isUploaded: false),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("3"), filename: "file3.txt", isUploaded: true)
        ]

        observer.didEnumerate(mixedItems)
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == true, "Should report dirty data when at least one item is not uploaded")
    }

    @Test("Enumerate with all non-uploaded items should report dirty data")
    func enumerateWithAllNonUploadedItems() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let nonUploadedItems = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: false),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("2"), filename: "file2.txt", isUploaded: false)
        ]

        observer.didEnumerate(nonUploadedItems)
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == true, "Should report dirty data when all items are not uploaded")
    }

    @Test("Multiple enumeration batches with uploaded items should report no dirty data")
    func multipleEnumerationBatchesWithUploadedItems() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let batch1 = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("2"), filename: "file2.txt", isUploaded: true)
        ]

        let batch2 = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("3"), filename: "file3.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("4"), filename: "file4.txt", isUploaded: true)
        ]

        observer.didEnumerate(batch1)
        observer.didEnumerate(batch2)
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == false, "Should report no dirty data when all batches contain only uploaded items")
    }

    @Test("Multiple enumeration batches with dirty data in second batch")
    func multipleEnumerationBatchesWithDirtyDataInSecondBatch() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let batch1 = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("2"), filename: "file2.txt", isUploaded: true)
        ]

        let batch2 = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("3"), filename: "file3.txt", isUploaded: false),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("4"), filename: "file4.txt", isUploaded: true)
        ]

        observer.didEnumerate(batch1)
        observer.didEnumerate(batch2)
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == true, "Should report dirty data when any batch contains non-uploaded items")
    }

    @Test("Finish enumerating with error should call completion handler")
    func finishEnumeratingWithError() {
        var hasDirtyData: Bool?
        var completionCallCount = 0

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
            completionCallCount += 1
        }

        let uploadedItems = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true)
        ]

        observer.didEnumerate(uploadedItems)

        let testError = NSError(domain: "TestError", code: 1, userInfo: nil)
        observer.finishEnumeratingWithError(testError)

        #expect(hasDirtyData == false, "Should report no dirty data even with error when all items are uploaded")
        #expect(completionCallCount == 1, "Completion handler should be called exactly once")
    }

    @Test("Finish enumerating with error and dirty data should report dirty data")
    func finishEnumeratingWithErrorAndDirtyData() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let nonUploadedItems = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: false)
        ]

        observer.didEnumerate(nonUploadedItems)

        let testError = NSError(domain: "TestError", code: 1, userInfo: nil)
        observer.finishEnumeratingWithError(testError)

        #expect(hasDirtyData == true, "Should report dirty data even with error when non-uploaded items exist")
    }

    @Test("Empty enumeration batch should not affect result")
    func emptyEnumerationBatch() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        observer.didEnumerate([])
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == false, "Should report no dirty data when only empty batches are enumerated")
    }

    @Test("Finish enumerating with next page should still complete")
    func finishEnumeratingWithNextPage() throws {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        let uploadedItems = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true)
        ]

        observer.didEnumerate(uploadedItems)

        let nextPage = try NSFileProviderPage(#require("nextPageToken".data(using: .utf8)))
        observer.finishEnumerating(upTo: nextPage)

        #expect(hasDirtyData == false, "Should complete with no dirty data when next page exists but all items are uploaded")
    }

    @Test("Early return optimization when dirty data found")
    func earlyReturnOptimization() {
        var hasDirtyData: Bool?

        let observer = DirtyUserDataObserver(log: mockLog) { result in
            hasDirtyData = result
        }

        // First item is not uploaded, should trigger early return
        let items = [
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: false),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("2"), filename: "file2.txt", isUploaded: true),
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("3"), filename: "file3.txt", isUploaded: true)
        ]

        observer.didEnumerate(items)
        observer.finishEnumerating(upTo: nil)

        #expect(hasDirtyData == true, "Should report dirty data when first item is not uploaded")
    }

    @Test("Completion handler is called only once")
    func completionHandlerCalledOnce() {
        var completionCallCount = 0

        let observer = DirtyUserDataObserver(log: mockLog) { _ in
            completionCallCount += 1
        }

        observer.didEnumerate([
            MockFileProviderItem(identifier: NSFileProviderItemIdentifier("1"), filename: "file1.txt", isUploaded: true)
        ])

        observer.finishEnumerating(upTo: nil)

        #expect(completionCallCount == 1, "Completion handler should be called exactly once")
    }
}
