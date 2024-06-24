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

struct MacCrafter: ParsableCommand {
    static let configuration = CommandConfiguration(
        abstract: "A tool to easily build a fully-functional Nextcloud Desktop Client for macOS."
    )

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

    @Option(name: [.short, .customLong("buildPath")], help: "Path for build files to be written.")
    var buildPath = "\(FileManager.default.currentDirectoryPath)/build"

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
        "https://github.com/sparkle-project/Sparkle/releases/download/1.27.3/Sparkle-1.27.3.tar.xz"

    @Option(name: [.long], help: "Git clone command; include options such as depth.")
    var gitCloneCommand = "git clone --depth=1"

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
        let craftTarget = "macos-clang-arm64"
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
            "\(craftBlueprintName).buildTests=\(buildTests ? "True" : "False")",
            "\(craftBlueprintName).buildMacOSBundle=\(disableAppBundle ? "False" : "True")",
            "\(craftBlueprintName).buildFileProviderModule=\(buildFileProviderModule ? "True" : "False")"
        ]

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

        print("Crafting Nextcloud Desktop Client...")

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

        guard let codeSignIdentity else {
            print("Crafted Nextcloud Desktop Client. Not codesigned.")
            return
        }

        print("Code-signing Nextcloud Desktop Client libraries and frameworks...")


        let clientAppDir = "\(clientBuildDir)/image-\(buildType)-master/\(appName).app"
        try codesignClientAppBundle(at: clientAppDir, withCodeSignIdentity: codeSignIdentity)

        print("Done!")
    }
}

MacCrafter.main()
