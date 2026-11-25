//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

final class MockHTTPURLProtocol: URLProtocol, @unchecked Sendable {
    private typealias Handlers = (
        start: @Sendable (MockHTTPURLProtocol) -> Void,
        stop: @Sendable (URLRequest) -> Void
    )

    private static let lock = NSLock()
    private nonisolated(unsafe) static var handlersByHost: [String: Handlers] = [:]
    private nonisolated(unsafe) static var responses: [URL: (statusCode: Int, body: Data, requests: [URLRequest])] = [:]

    private var handlers: Handlers? {
        guard let host = request.url?.host else { return nil }
        return Self.lock.withLock { Self.handlersByHost[host] }
    }

    static func setHandlers(
        forHost host: String,
        start: @escaping @Sendable (MockHTTPURLProtocol) -> Void,
        stop: @escaping @Sendable (URLRequest) -> Void
    ) {
        lock.withLock {
            handlersByHost[host] = (start, stop)
        }
    }

    static func removeHandlers(forHost host: String) {
        lock.withLock {
            _ = handlersByHost.removeValue(forKey: host)
        }
    }

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
        if let handlers {
            handlers.start(self)
            return
        }

        guard let url = request.url,
              let response = Self.lock.withLock({ () -> (Int, Data)? in
                  guard var stored = Self.responses[url] else { return nil }
                  stored.requests.append(request)
                  Self.responses[url] = stored
                  return (stored.statusCode, stored.body)
              })
        else {
            client?.urlProtocol(self, didFailWithError: URLError(.unsupportedURL))
            return
        }

        respond(status: response.0, headers: ["Content-Type": "application/json"], body: response.1)
    }

    func respond(status: Int, headers: [String: String] = [:], body: Data = Data()) {
        guard let url = request.url,
              let response = HTTPURLResponse(url: url, statusCode: status, httpVersion: "HTTP/1.1", headerFields: headers)
        else {
            client?.urlProtocol(self, didFailWithError: URLError(.badServerResponse))
            return
        }

        client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: body)
        client?.urlProtocolDidFinishLoading(self)
    }

    override func stopLoading() {
        handlers?.stop(request)
    }
}
