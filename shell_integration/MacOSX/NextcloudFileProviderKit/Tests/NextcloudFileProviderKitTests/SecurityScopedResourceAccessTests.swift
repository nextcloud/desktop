//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct SecurityScopedResourceAccessTests {
    @Test func replacementKeepsScopeActiveDuringOperationAndStopsItOnInvalidate() throws {
        let url = URL(fileURLWithPath: "/external-state")
        var activeURLs = Set<URL>()
        var stops: [URL] = []
        let access = SecurityScopedResourceAccess(
            startAccess: {
                activeURLs.insert($0)
                return true
            },
            stopAccess: {
                activeURLs.remove($0)
                stops.append($0)
            }
        )

        let result = try access.replacingAccess(with: url) {
            #expect(activeURLs.contains(url))
            return 42
        }

        #expect(result == 42)
        #expect(activeURLs == [url])

        access.invalidate()
        #expect(activeURLs.isEmpty)
        #expect(stops == [url])
    }

    @Test func replacingScopeStopsPreviousScopeAfterOperation() throws {
        let first = URL(fileURLWithPath: "/first")
        let second = URL(fileURLWithPath: "/second")
        var activeURLs = Set<URL>()
        var stops: [URL] = []
        let access = SecurityScopedResourceAccess(
            startAccess: {
                activeURLs.insert($0)
                return true
            },
            stopAccess: {
                activeURLs.remove($0)
                stops.append($0)
            }
        )

        _ = try access.replacingAccess(with: first) { () }
        _ = try access.replacingAccess(with: second) {
            #expect(activeURLs.contains(first))
            #expect(activeURLs.contains(second))
        }

        #expect(activeURLs == [second])
        #expect(stops == [first])
    }

    @Test func invalidationBeforeReplacementPreventsAccessAndOperation() throws {
        var startCount = 0
        var operationRan = false
        let access = SecurityScopedResourceAccess(
            startAccess: { _ in
                startCount += 1
                return true
            },
            stopAccess: { _ in }
        )
        access.invalidate()

        let result: Int? = try access.replacingAccess(with: URL(fileURLWithPath: "/state")) {
            operationRan = true
            return 1
        }

        #expect(result == nil)
        #expect(startCount == 0)
        #expect(!operationRan)
    }

    @Test func failedReplacementBalancesNewScopeAndKeepsPreviousScope() throws {
        let first = URL(fileURLWithPath: "/first")
        let second = URL(fileURLWithPath: "/second")
        var activeURLs = Set<URL>()
        let access = SecurityScopedResourceAccess(
            startAccess: {
                activeURLs.insert($0)
                return true
            },
            stopAccess: { activeURLs.remove($0) }
        )

        _ = try access.replacingAccess(with: first) { () }

        #expect(throws: CocoaError.self) {
            _ = try access.replacingAccess(with: second) {
                throw CocoaError(.fileWriteUnknown)
            } as Void?
        }

        #expect(activeURLs == [first])
    }

    @Test func deniedScopeDoesNotRunOperation() {
        var operationRan = false
        let access = SecurityScopedResourceAccess(
            startAccess: { _ in false },
            stopAccess: { _ in }
        )

        #expect(throws: CocoaError.self) {
            _ = try access.replacingAccess(with: URL(fileURLWithPath: "/state")) {
                operationRan = true
            } as Void?
        }

        #expect(!operationRan)
    }
}
