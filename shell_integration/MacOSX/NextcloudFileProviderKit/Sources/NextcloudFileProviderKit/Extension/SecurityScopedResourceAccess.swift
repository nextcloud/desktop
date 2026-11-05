//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

/// Keeps one security-scoped URL active while code that depends on it is in use.
final class SecurityScopedResourceAccess: @unchecked Sendable {
    private let lock = NSLock()
    private let startAccess: (URL) -> Bool
    private let stopAccess: (URL) -> Void
    private var currentURL: URL?
    private var invalidated = false

    init(
        startAccess: @escaping (URL) -> Bool = { $0.startAccessingSecurityScopedResource() },
        stopAccess: @escaping (URL) -> Void = { $0.stopAccessingSecurityScopedResource() }
    ) {
        self.startAccess = startAccess
        self.stopAccess = stopAccess
    }

    /// Replaces the active security scope and performs `operation` before invalidation can stop it.
    ///
    /// Returns `nil` when the holder was already invalidated.
    func replacingAccess<T>(with url: URL?, perform operation: () throws -> T) throws -> T? {
        lock.lock()

        guard !invalidated else {
            lock.unlock()
            return nil
        }

        let previousURL = currentURL
        let shouldStartAccess = url != nil && url != previousURL

        if shouldStartAccess, let url, !startAccess(url) {
            lock.unlock()
            throw CocoaError(.fileReadNoPermission)
        }

        currentURL = url

        do {
            let result = try operation()
            lock.unlock()

            if previousURL != url, let previousURL {
                stopAccess(previousURL)
            }

            return result
        } catch {
            currentURL = previousURL
            lock.unlock()

            if shouldStartAccess, let url {
                stopAccess(url)
            }

            throw error
        }
    }

    func invalidate() {
        lock.lock()
        invalidated = true
        let url = currentURL
        currentURL = nil
        lock.unlock()

        if let url {
            stopAccess(url)
        }
    }
}
