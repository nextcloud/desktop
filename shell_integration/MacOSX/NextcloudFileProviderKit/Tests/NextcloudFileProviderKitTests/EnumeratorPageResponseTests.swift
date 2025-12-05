//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

@Suite struct EnumeratorPageResponseTests {
    private func createMockAFDataResponse(
        headers: [String: String]?,
        statusCode: Int = 200,
        data: Data? = Data()
    ) -> AFDataResponse<Data>? {
        guard let url = URL(string: "https://example.com") else {
            print("Error: Failed to create URL in test helper.")
            return nil
        }
        let httpResponse = HTTPURLResponse(
            url: url,
            statusCode: statusCode,
            httpVersion: "HTTP/1.1",
            headerFields: headers
        )
        let result: Result<Data, AFError> = .success(data ?? Data())
        return AFDataResponse<Data>(
            request: nil,
            response: httpResponse,
            data: data,
            metrics: nil,
            serializationDuration: 0,
            result: result
        )
    }

    // MARK: - Success Cases

    @Test("Init with valid headers and total succeeds")
    func initWithValidHeadersAndTotal() {
        let headers = [
            "X-NC-PAGINATE": "true",
            "X-NC-PAGINATE-TOKEN": "nextToken123",
            "X-NC-PAGINATE-TOTAL": "100"
        ]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())

        #expect(enumeratorResponse != nil, "Initialization should succeed with valid values.")
        #expect(enumeratorResponse?.token == "nextToken123")
        #expect(enumeratorResponse?.index == 0)
        #expect(enumeratorResponse?.total == 100)
    }

    @Test("Init with valid headers and missing total succeeds")
    func initWithValidHeadersAndMissingTotal() {
        let headers = ["X-NC-PAGINATE": "true", "X-NC-PAGINATE-TOKEN": "anotherToken456"]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 1

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())

        #expect(enumeratorResponse != nil, "Initialization should succeed with valid values.")
        #expect(enumeratorResponse?.token == "anotherToken456")
        #expect(enumeratorResponse?.index == 1)
        #expect(enumeratorResponse?.total == nil, "Total should be nil when the header is missing.")
    }

    @Test("Init with case-insensitive header keys and 'TRUE' succeeds")
    func initWithCaseInsensitiveHeaders() {
        let headers = [
            "x-nc-paginate": "TRUE", // Lowercase key, uppercase value for boolean
            "x-nc-paginate-token": "mixedCaseToken789",
            "X-NC-PAGINATE-TOTAL": "50"
        ]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 2

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())

        #expect(enumeratorResponse != nil, "Init should succeed with case-insensitive headers.")
        #expect(enumeratorResponse?.token == "mixedCaseToken789")
        #expect(enumeratorResponse?.index == 2)
        #expect(enumeratorResponse?.total == 50)
    }

    @Test("Init with non-integer total value results in nil total")
    func initWithNonIntegerTotal() {
        let headers = [
            "X-NC-PAGINATE": "true",
            "X-NC-PAGINATE-TOKEN": "tokenWithInvalidTotal",
            "X-NC-PAGINATE-TOTAL": "not-an-integer"
        ]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 3

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())

        #expect(enumeratorResponse != nil, "Init should succeed even if total is not valid integer")
        #expect(enumeratorResponse?.token == "tokenWithInvalidTotal")
        #expect(enumeratorResponse?.index == 3)
        #expect(enumeratorResponse?.total == nil, "Total should be nil if cannot be parsed as Int")
    }

    // MARK: - Failure Cases

    @Test("Init with nil nkResponseData returns nil")
    func initWithNilNkResponseData() {
        let index = 0
        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: nil, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if nkResponseData is nil.")
    }

    @Test("Init with nil HTTPURLResponse returns nil")
    func initWithNilHttpResponse() {
        let afResponseWithNilHttp = AFDataResponse<Data>(
            request: nil,
            response: nil, // HTTPURLResponse is nil
            data: Data(),
            metrics: nil,
            serializationDuration: 0,
            result: .success(Data())
        )
        let index = 0
        let enumeratorResponse =
            EnumeratorPageResponse(nkResponseData: afResponseWithNilHttp, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if HTTPURLResponse is nil.")
    }

    @Test("Init with empty headers returns nil")
    func initWithEmptyHeaders() {
        let mockResponse = createMockAFDataResponse(headers: [:])
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if required headers empty")
    }

    @Test("Init without X-NC-PAGINATE header returns nil")
    func initWithoutPaginateHeader() {
        let headers = ["X-NC-PAGINATE-TOKEN": "someToken"]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if PAGINATE header missing.")
    }

    @Test("Init with X-NC-PAGINATE header set to 'false' returns nil")
    func initWithPaginateHeaderFalse() {
        let headers = [
            "X-NC-PAGINATE": "false",
            "X-NC-PAGINATE-TOKEN": "someToken"
        ]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if PAGINATE header is false")
    }

    @Test("Init with X-NC-PAGINATE header not a valid 'true' string returns nil")
    func initWithPaginateHeaderNotTrueString() {
        let headers = ["X-NC-PAGINATE": "false", "X-NC-PAGINATE-TOKEN": "someToken"]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if PAGINATE header not true")
    }

    @Test("Init without X-NC-PAGINATE-TOKEN header returns nil")
    func initWithoutPaginateTokenHeader() {
        let headers = ["X-NC-PAGINATE": "true"]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse == nil, "Initialization should fail if TOKEN header is missing.")
    }

    @Test("Init with empty X-NC-PAGINATE-TOKEN header succeeds with empty token")
    func initWithEmptyPaginateToken() {
        // The current implementation allows an empty token if the header key exists.
        let headers = ["X-NC-PAGINATE": "true", "X-NC-PAGINATE-TOKEN": ""]
        let mockResponse = createMockAFDataResponse(headers: headers)
        let index = 0

        let enumeratorResponse = EnumeratorPageResponse(nkResponseData: mockResponse, index: index, log: FileProviderLogMock())
        #expect(enumeratorResponse != nil, "Initialization should succeed with empty token string.")
        #expect(enumeratorResponse?.token == "", "Token should be an empty string.")
        #expect(enumeratorResponse?.index == 0)
        #expect(enumeratorResponse?.total == nil)
    }
}
