//
//  UploadTests.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2025-01-07.
//

import XCTest
@testable import NextcloudFileProviderKit

final class UploadTests: XCTestCase {

    func testSucceededUploadResult() {
        let uploadResult = UploadResult(
            ocId: nil,
            chunks: nil,
            etag: nil,
            date: nil,
            size: nil,
            afError: nil,
            remoteError: .success
        )
        XCTAssertTrue(uploadResult.succeeded)
    }
}
