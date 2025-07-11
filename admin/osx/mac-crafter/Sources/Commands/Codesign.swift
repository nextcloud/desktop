// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

struct Codesign: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Codesigning script for the client.")
    
    @Argument(help: "Path to the Nextcloud Desktop Client app bundle.")
    var appBundlePath = "\(FileManager.default.currentDirectoryPath)/product/Nextcloud.app"
    
    @Option(name: [.short, .long], help: "Code signing identity for desktop client and libs.")
    var codeSignIdentity: String
    
    @Option(name: [.short, .long], help: "Entitlements to apply to the app bundle.")
    var entitlementsPath: String?
    
    mutating func run() async throws {
        let absolutePath = appBundlePath.hasPrefix("/")
        ? appBundlePath
        : "\(FileManager.default.currentDirectoryPath)/\(appBundlePath)"
        
        try await codesignClientAppBundle(
            at: absolutePath,
            withCodeSignIdentity: codeSignIdentity,
            usingEntitlements: entitlementsPath
        )
    }
}
