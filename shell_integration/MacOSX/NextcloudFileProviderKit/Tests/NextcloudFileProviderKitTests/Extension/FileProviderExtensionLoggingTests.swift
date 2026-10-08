//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudKit
import Testing

///
/// NextcloudKit's logger configuration is not thread-safe. The extension configures it once per process, so instances created concurrently, as the test suites do, do not race in it.
///
@Suite("Extension logging")
struct FileProviderExtensionLoggingTests {
    @Test func extensionsCreatedConcurrentlyShareOneLoggingConfiguration() async {
        let logFileURLs = await withTaskGroup(of: URL?.self) { group in
            for index in 0 ..< 16 {
                group.addTask {
                    let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier("logging-\(index)"), displayName: "Logging \(index)")
                    _ = FileProviderExtension(domain: domain)
                    return NKLogFileManager.shared.currentLogFileURL()
                }
            }
            var urls: [URL?] = []
            for await url in group {
                urls.append(url)
            }
            return urls
        }

        #expect(logFileURLs.count == 16)
        #expect(Set(logFileURLs.map { $0?.path ?? "" }).count == 1, "Every instance logs through the one configuration.")
        #if DEBUG
            #expect(NKLogFileManager.shared.logLevel == .verbose)
        #else
            #expect(NKLogFileManager.shared.logLevel == .normal)
        #endif
    }
}
