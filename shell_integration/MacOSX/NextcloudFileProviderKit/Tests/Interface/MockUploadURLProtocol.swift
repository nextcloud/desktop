// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

/// Supplies per-host responses for upload tests, including requests held until cancellation.
public final class MockUploadURLProtocol: URLProtocol, @unchecked Sendable {
    private static let lock = NSLock()
    private nonisolated(unsafe) static var handlers: [String: @Sendable (MockUploadURLProtocol) -> Void] = [:]

    public static func setHandler(forHost host: String, handler: (@Sendable (MockUploadURLProtocol) -> Void)?) {
        lock.lock()
        defer { lock.unlock() }
        handlers[host] = handler
    }

    override public class func canInit(with request: URLRequest) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return handlers[request.url?.host ?? ""] != nil
    }

    override public class func canonicalRequest(for request: URLRequest) -> URLRequest {
        request
    }

    override public func startLoading() {
        Self.lock.lock()
        let handler = Self.handlers[request.url?.host ?? ""]
        Self.lock.unlock()
        handler?(self)
    }

    override public func stopLoading() {}

    public func respond(status: Int, headers: [String: String] = [:], data: Data = Data()) {
        guard let url = request.url,
              let response = HTTPURLResponse(url: url, statusCode: status, httpVersion: "HTTP/1.1", headerFields: headers)
        else {
            client?.urlProtocol(self, didFailWithError: URLError(.badServerResponse))
            return
        }
        client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: data)
        client?.urlProtocolDidFinishLoading(self)
    }
}
