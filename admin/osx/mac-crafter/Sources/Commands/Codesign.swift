// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

struct Codesign: AsyncParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Codesigning script for the client.")
    
    @Argument(help: "Path to the Nextcloud desktop client app bundle.")
    var appBundlePath = "\(FileManager.default.currentDirectoryPath)/product/Nextcloud.app"
    
    @Option(name: [.short, .long], help: "Code signing identity for desktop client and libs.")
    var codeSignIdentity: String
    
    @Flag(help: "Produce a developer build.")
    var developerBuild = false
    
    mutating func run() async throws {
        let absolutePath = appBundlePath.hasPrefix("/") ? appBundlePath : "\(FileManager.default.currentDirectoryPath)/\(appBundlePath)"
        let url = URL(fileURLWithPath: absolutePath)
        try await Signer.signMainBundle(at: url, codeSignIdentity: codeSignIdentity, developerBuild: developerBuild)
    }
}
