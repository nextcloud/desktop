// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

struct CreateDMG: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Create a DMG for the client.")
    
    @Argument(help: "Path to the desktop client app bundle.")
    var appBundlePath: String
    
    @Option(name: [.short, .long], help: "Path for the final product to be put.")
    var productPath = "\(FileManager.default.currentDirectoryPath)/product"
    
    @Option(name: [.short, .long], help: "Path for build files to be written.")
    var buildPath = "\(FileManager.default.currentDirectoryPath)/build"
    
    @Option(name: [.long], help: "The application's branded name.")
    var appName = "Nextcloud"
    
    @Option(name: [.long], help: "Apple ID, used for notarisation.")
    var appleId: String?
    
    @Option(name: [.long], help: "Apple ID password, used for notarisation.")
    var applePassword: String?
    
    @Option(name: [.long], help: "Apple Team ID, used for notarisation.")
    var appleTeamId: String?
    
    @Option(name: [.long], help: "Apple package signing ID.")
    var packageSigningId: String?
    
    @Option(name: [.long], help: "Sparkle package signing key.")
    var sparklePackageSignKey: String?
    
    mutating func run() async throws {
        try await installIfMissing("create-dmg", "brew install create-dmg")
        try await createDmgForAppBundle(
            appBundlePath: appBundlePath,
            productPath: productPath,
            buildPath: buildPath,
            appName: appName,
            packageSigningId: packageSigningId,
            appleId: appleId,
            applePassword: applePassword,
            appleTeamId: appleTeamId,
            sparklePackageSignKey: sparklePackageSignKey
        )
    }
}
