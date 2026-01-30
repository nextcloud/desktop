// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

struct Codesign: AsyncParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Codesigning script for the client.")
    
    @Argument(help: "Path to the Nextcloud desktop client app bundle.")
    var appBundlePath: String

    @Argument(help: "Code signing identity for desktop client and libs.")
    var codeSignIdentity: String

    @Argument(help: "Location of the entitlements manifest for the app.")
    var appEntitlements: String

    @Argument(help: "Location of the entitlements manifest for the file provider extension.")
    var fileProviderEntitlements: String

    @Argument(help: "Location of the entitlements manifest for the file provider UI extension.")
    var fileProviderUIEntitlements: String

    @Argument(help: "Location of the entitlements manifest for the Finder sync extension.")
    var finderSyncEntitlements: String

    mutating func run() async throws {
        let absolutePath = appBundlePath.hasPrefix("/") ? appBundlePath : "\(FileManager.default.currentDirectoryPath)/\(appBundlePath)"
        let url = URL(fileURLWithPath: absolutePath)

        let entitlements = [
            url.lastPathComponent: URL(fileURLWithPath: appEntitlements),
            "FileProviderExt.appex": URL(fileURLWithPath: fileProviderEntitlements),
            "FileProviderUIExt.appex": URL(fileURLWithPath: fileProviderUIEntitlements),
            "FinderSyncExt.appex": URL(fileURLWithPath: finderSyncEntitlements),
        ]

        try await Signer.signMainBundle(at: url, codeSignIdentity: codeSignIdentity, entitlements: entitlements)
    }
}
