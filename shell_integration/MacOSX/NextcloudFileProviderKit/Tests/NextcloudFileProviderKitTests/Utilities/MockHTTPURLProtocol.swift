//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

final class MockHTTPURLProtocol: URLProtocol, @unchecked Sendable {
    private static let lock = NSLock()
    private nonisolated(unsafe) static var responses: [URL: (statusCode: Int, body: Data, requests: [URLRequest])] = [:]

    static func setResponse(for url: URL, statusCode: Int, body: Data) {
        lock.withLock {
            responses[url] = (statusCode, body, [])
        }
    }

    static func requests(for url: URL) -> [URLRequest] {
        lock.withLock {
            responses[url]?.requests ?? []
        }
    }

    static func removeResponse(for url: URL) {
        lock.withLock {
            _ = responses.removeValue(forKey: url)
        }
    }

    override class func canInit(with _: URLRequest) -> Bool {
        true
    }

    override class func canonicalRequest(for request: URLRequest) -> URLRequest {
        request
    }

    override func startLoading() {
        guard let url = request.url,
              let response = Self.lock.withLock({ () -> (Int, Data)? in
                  guard var stored = Self.responses[url] else { return nil }
                  stored.requests.append(request)
                  Self.responses[url] = stored
                  return (stored.statusCode, stored.body)
              }),
              let httpResponse = HTTPURLResponse(
                  url: url,
                  statusCode: response.0,
                  httpVersion: "HTTP/1.1",
                  headerFields: ["Content-Type": "application/json"]
              )
        else {
            client?.urlProtocol(self, didFailWithError: URLError(.unsupportedURL))
            return
        }

        client?.urlProtocol(self, didReceive: httpResponse, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: response.1)
        client?.urlProtocolDidFinishLoading(self)
    }

    override func stopLoading() {}
}
