/*
 * Copyright (C) 2024 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import ArgumentParser
import Foundation

struct Build: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Client building script")

    enum MacCrafterError: Error {
        case failedEnumeration(String)
        case environmentError(String)
        case gitError(String)
        case craftError(String)
    }

    @Argument(help: "Path to the root directory of the Nextcloud Desktop Client git repository.")
    var repoRootDir = "\(FileManager.default.currentDirectoryPath)/../../.."

    @Option(name: [.short, .long], help: "Code signing identity for desktop client and libs.")
    var codeSignIdentity: String?

    @Option(name: [.short, .long], help: "Path for build files to be written.")
    var buildPath = "\(FileManager.default.currentDirectoryPath)/build"

    @Option(name: [.short, .long], help: "Path for the final product to be put.")
    var productPath = "\(FileManager.default.currentDirectoryPath)/product"

    @Option(name: [.short, .long], help: "Architecture.")
    var arch = "arm64"

    @Option(name: [.long], help: "Brew installation script URL.")
    var brewInstallShUrl = "https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh"

    @Option(name: [.long], help: "CraftMaster git url.")
    var craftMasterGitUrl = "https://invent.kde.org/packaging/craftmaster.git"

    @Option(name: [.long], help: "Nextcloud Desktop Client craft blueprint git url.")
    var clientBlueprintsGitUrl = "https://github.com/nextcloud/desktop-client-blueprints.git"

    @Option(name: [.long], help: "Nextcloud Desktop Client craft blueprint name.")
    var craftBlueprintName = "nextcloud-client"

    @Option(name: [.long], help: "Build type (e.g. Release, RelWithDebInfo, MinSizeRel, Debug).")
    var buildType = "RelWithDebInfo"

    @Option(name: [.long], help: "The application's branded name.")
    var appName = "Nextcloud"

    @Option(name: [.long], help: "Sparkle download URL.")
    var sparkleDownloadUrl =
        "https://github.com/sparkle-project/Sparkle/releases/download/2.6.4/Sparkle-2.6.4.tar.xz"

    @Option(name: [.long], help: "Git clone command; include options such as depth.")
    var gitCloneCommand = "git clone --depth=1"

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

    @Option(name: [.long], help: "Override server url.")
    var overrideServerUrl: String?

    @Flag(help: "Reconfigure KDE Craft.")
    var reconfigureCraft = false

    @Flag(help: "Run build offline (i.e. do not update craft)")
    var offline = false

    @Flag(help: "Build test suite.")
    var buildTests = false

    @Flag(name: [.long], help: "Do not build App Bundle.")
    var disableAppBundle = false

    @Flag(help: "Build File Provider Module.")
    var buildFileProviderModule = false

    @Flag(help: "Build without Sparkle auto-updater.")
    var disableAutoUpdater = false

    @Flag(help: "Run a full rebuild.")
    var fullRebuild = false

    @Flag(help: "Force override server URL.")
    var forceOverrideServerUrl = false

    @Flag(help: "Create an installer package.")
    var package = false

    @Flag(help: "Build in developer mode.")
    var dev = false

    mutating func run() throws {
        print("Configuring build tooling.")

        if codeSignIdentity != nil {
            guard commandExists("codesign") else {
                throw MacCrafterError.environmentError("codesign not found, cannot proceed.")
            }
        }

        try installIfMissing("git", "xcode-select --install")
        try installIfMissing(
            "brew",
            "curl -fsSL \(brewInstallShUrl) | /bin/bash",
            installCommandEnv: ["NONINTERACTIVE": "1"]
        )
        try installIfMissing("inkscape", "brew install inkscape")
        try installIfMissing("python3", "brew install pyenv && pyenv install 3.12.4")

        print("Build tooling configured.")

        let fm = FileManager.default
        let craftMasterDir = "\(buildPath)/craftmaster"
        let craftMasterIni = "\(repoRootDir)/craftmaster.ini"
        let craftMasterPy = "\(craftMasterDir)/CraftMaster.py"
        let craftTarget = archToCraftTarget(arch)
        let craftCommand =
            "python3 \(craftMasterPy) --config \(craftMasterIni) --target \(craftTarget) -c"

        if !fm.fileExists(atPath: craftMasterDir) || reconfigureCraft {
            print("Configuring KDE Craft.")

            if fm.fileExists(atPath: craftMasterDir) {
                print("KDE Craft is already cloned.")
            } else {
                print("Cloning KDE Craft...")
                guard shell("\(gitCloneCommand) \(craftMasterGitUrl) \(craftMasterDir)") == 0 else {
                    throw MacCrafterError.gitError("Error cloning craftmaster.")
                }
            }

            print("Configuring Nextcloud Desktop Client blueprints for KDE Craft...")
            guard shell("\(craftCommand) --add-blueprint-repository \(clientBlueprintsGitUrl)") == 0 else {
                throw MacCrafterError.craftError("Error adding blueprint repository.")
            }

            print("Crafting KDE Craft...")
            guard shell("\(craftCommand) craft") == 0 else {
                throw MacCrafterError.craftError("Error crafting KDE Craft.")
            }

            print("Crafting Nextcloud Desktop Client dependencies...")
            guard shell("\(craftCommand) --install-deps \(craftBlueprintName)") == 0 else {
                throw MacCrafterError.craftError("Error installing dependencies.")
            }
        }

        var craftOptions = [
            "\(craftBlueprintName).srcDir=\(repoRootDir)",
            "\(craftBlueprintName).osxArchs=\(arch)",
            "\(craftBlueprintName).buildTests=\(buildTests ? "True" : "False")",
            "\(craftBlueprintName).buildMacOSBundle=\(disableAppBundle ? "False" : "True")",
            "\(craftBlueprintName).buildFileProviderModule=\(buildFileProviderModule ? "True" : "False")"
        ]

        if let overrideServerUrl {
            craftOptions.append("\(craftBlueprintName).overrideServerUrl=\(overrideServerUrl)")
            craftOptions.append("\(craftBlueprintName).forceOverrideServerUrl=\(forceOverrideServerUrl ? "True" : "False")")
        }

        if dev {
            appName += "Dev"
            craftOptions.append("\(craftBlueprintName).devMode=True")
        }

        if !disableAutoUpdater {
            print("Configuring Sparkle auto-updater.")

            let fm = FileManager.default
            guard fm.fileExists(atPath: "\(buildPath)/Sparkle.tar.xz") ||
                  shell("wget \(sparkleDownloadUrl) -O \(buildPath)/Sparkle.tar.xz") == 0
            else {
                throw MacCrafterError.environmentError("Error downloading sparkle.")
            }

            guard fm.fileExists(atPath: "\(buildPath)/Sparkle.framework") ||
                  shell("tar -xvf \(buildPath)/Sparkle.tar.xz -C \(buildPath)") == 0
            else {
                throw MacCrafterError.environmentError("Error unpacking sparkle.")
            }

            craftOptions.append(
                "\(craftBlueprintName).sparkleLibPath=\(buildPath)/Sparkle.framework"
            )
        }

        print("Crafting \(appName) Desktop Client...")

        let allOptionsString = craftOptions.map({ "--options \"\($0)\"" }).joined(separator: " ")

        let clientBuildDir = "\(buildPath)/\(craftTarget)/build/\(craftBlueprintName)"
        if fullRebuild {
            do {
                try fm.removeItem(atPath: clientBuildDir)
            } catch let error {
                print("WARNING! Error removing build directory: \(error)")
            }
        }

        let buildMode = fullRebuild ? "-i" : disableAppBundle ? "compile" : "--compile --install"
        let offlineMode = offline ? "--offline" : ""
        guard shell(
            "\(craftCommand) --buildtype \(buildType) \(buildMode) \(offlineMode) \(allOptionsString) \(craftBlueprintName)"
        ) == 0 else {
            throw MacCrafterError.craftError("Error crafting Nextcloud Desktop Client.")
        }

        let clientAppDir = "\(clientBuildDir)/image-\(buildType)-master/\(appName).app"
        if let codeSignIdentity {
            print("Code-signing Nextcloud Desktop Client libraries and frameworks...")
            try codesignClientAppBundle(at: clientAppDir, withCodeSignIdentity: codeSignIdentity)
        }

        print("Placing Nextcloud Desktop Client in \(productPath)...")
        if !fm.fileExists(atPath: productPath) {
            try fm.createDirectory(
                atPath: productPath, withIntermediateDirectories: true, attributes: nil
            )
        }
        if fm.fileExists(atPath: "\(productPath)/\(appName).app") {
            try fm.removeItem(atPath: "\(productPath)/\(appName).app")
        }
        try fm.copyItem(atPath: clientAppDir, toPath: "\(productPath)/\(appName).app")

        if package {
            try packageAppBundle(
                productPath: productPath,
                buildPath: buildPath,
                craftTarget: craftTarget,
                craftBlueprintName: craftBlueprintName,
                appName: appName,
                packageSigningId: packageSigningId,
                appleId: appleId,
                applePassword: applePassword,
                appleTeamId: appleTeamId,
                sparklePackageSignKey: sparklePackageSignKey
            )
        }

        print("Done!")
    }
}

struct Codesign: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Codesigning script for the client.")

    @Argument(help: "Path to the Nextcloud Desktop Client app bundle.")
    var appBundlePath = "\(FileManager.default.currentDirectoryPath)/product/Nextcloud.app"

    @Option(name: [.short, .long], help: "Code signing identity for desktop client and libs.")
    var codeSignIdentity: String

    mutating func run() throws {
        let absolutePath = appBundlePath.hasPrefix("/")
            ? appBundlePath
            : "\(FileManager.default.currentDirectoryPath)/\(appBundlePath)"
        try codesignClientAppBundle(at: absolutePath, withCodeSignIdentity: codeSignIdentity)
    }
}

struct Package: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Packaging script for the client.")

    @Option(name: [.short, .long], help: "Architecture.")
    var arch = "arm64"

    @Option(name: [.short, .long], help: "Path for build files to be written.")
    var buildPath = "\(FileManager.default.currentDirectoryPath)/build"

    @Option(name: [.short, .long], help: "Path for the final product to be put.")
    var productPath = "\(FileManager.default.currentDirectoryPath)/product"

    @Option(name: [.long], help: "Nextcloud Desktop Client craft blueprint name.")
    var craftBlueprintName = "nextcloud-client"

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

    mutating func run() throws {
        try packageAppBundle(
            productPath: productPath,
            buildPath: buildPath,
            craftTarget: archToCraftTarget(arch),
            craftBlueprintName: craftBlueprintName,
            appName: appName,
            packageSigningId: packageSigningId,
            appleId: appleId,
            applePassword: applePassword,
            appleTeamId: appleTeamId,
            sparklePackageSignKey: sparklePackageSignKey
        )
    }
}

struct MacCrafter: ParsableCommand {
    static let configuration = CommandConfiguration(
        abstract: "A tool to easily build a fully-functional Nextcloud Desktop Client for macOS.",
        subcommands: [Build.self, Codesign.self, Package.self],
        defaultSubcommand: Build.self
    )
}

MacCrafter.main()
