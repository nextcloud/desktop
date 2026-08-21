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

        var entitlements: [String: URL] = [
            url.lastPathComponent: URL(fileURLWithPath: appEntitlements),
            "FileProviderExt.appex": URL(fileURLWithPath: fileProviderEntitlements),
            "FileProviderUIExt.appex": URL(fileURLWithPath: fileProviderUIEntitlements),
            "FinderSyncExt.appex": URL(fileURLWithPath: finderSyncEntitlements),
        ]

        // The FinderSync broker login item has a branded filename derived from its
        // bundle identifier, so it cannot be represented by a fixed dictionary key.
        // Discover login items from the app bundle and assign the broker entitlement
        // manifest to their actual wrapper filenames.
        let loginItemsLocation = url
            .appendingPathComponent("Contents")
            .appendingPathComponent("Library")
            .appendingPathComponent("LoginItems")

        if FileManager.default.fileExists(atPath: loginItemsLocation.path) {
            let loginItems = try FileManager.default.contentsOfDirectory(
                at: loginItemsLocation,
                includingPropertiesForKeys: nil
            ).filter {
                $0.pathExtension == "app"
            }

            if !loginItems.isEmpty {
                let loginItemEntitlements = URL(fileURLWithPath: finderSyncEntitlements)
                    .deletingLastPathComponent()
                    .appendingPathComponent("FinderSyncBroker.entitlements")

                guard FileManager.default.fileExists(atPath: loginItemEntitlements.path) else {
                    throw MacCrafterError.signing(
                        "Login item entitlement manifest does not exist: \(loginItemEntitlements.path)"
                    )
                }

                for loginItem in loginItems {
                    entitlements[loginItem.lastPathComponent] = loginItemEntitlements
                }
            }
        }
        try await Signer.signMainBundle(at: url, codeSignIdentity: codeSignIdentity, entitlements: entitlements)
    }
}
