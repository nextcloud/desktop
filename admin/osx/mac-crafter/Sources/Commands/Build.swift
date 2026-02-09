// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

struct Build: AsyncParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Client building script")

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
    
    @Option(name: [.long], help: "CraftMaster Git URL.")
    var craftMasterGitUrl = "https://invent.kde.org/packaging/craftmaster.git"
    
    @Option(name: [.long], help: "KDE Craft blueprints Git URL.")
    var kdeBlueprintsGitUrl = "https://github.com/nextcloud/craft-blueprints-kde.git"
    
    @Option(name: [.long], help: "KDE Craft blueprints git ref/branch")
    var kdeBlueprintsGitRef = "master"
    
    @Option(name: [.long], help: "Nextcloud Desktop Client craft blueprints Git URL.")
    var clientBlueprintsGitUrl = "https://github.com/nextcloud/desktop-client-blueprints.git"
    
    @Option(name: [.long], help: "Nextcloud Desktop Client craft blueprints Git ref/branch.")
    var clientBlueprintsGitRef = "master"
    
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
    
    @Option(name: [.long], help: "Override server URL.")
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
    
    ///
    /// Download the Sparkle framework archive with URLSession.
    ///
    private func downloadSparkle() async throws -> URL {
        guard let url = URL(string: sparkleDownloadUrl) else {
            throw MacCrafterError.downloadError("Sparkle download URL appears to be invalid: \(sparkleDownloadUrl)")
        }
        
        let request = URLRequest(url: url)
        let (file, _) = try await URLSession.shared.download(for: request)
        
        return file
    }
    
    mutating func run() async throws {
        let stopwatch = Stopwatch()

        // MARK: Dependencies

        Log.info("Ensuring build dependencies are met...")
        stopwatch.record("Build Dependencies")

        if codeSignIdentity != nil {
            guard await commandExists("codesign") else {
                throw MacCrafterError.environmentError("codesign command not found, cannot proceed!")
            }
        }

        try await installIfMissing("git", "xcode-select --install")
        try await installIfMissing(
            "brew",
            "curl -fsSL \(brewInstallShUrl) | /bin/bash",
            installCommandEnv: ["NONINTERACTIVE": "1"]
        )
        try await installIfMissing("wget", "brew install wget")
        try await installIfMissing("inkscape", "brew install inkscape")
        try await installIfMissing("python3", "brew install pyenv && pyenv install 3.12.4")
        
        Log.info("Build dependencies are installed.")

        // MARK: KDE Craft

        let fm = FileManager.default
        let buildURL = URL(fileURLWithPath: buildPath).standardized
        let repoRootURL = URL(fileURLWithPath: repoRootDir).standardized
        let craftMasterDir = buildURL.appendingPathComponent("craftmaster")
        let craftMasterIni = repoRootURL.appendingPathComponent("craftmaster.ini")
        let craftMasterPy = craftMasterDir.appendingPathComponent("CraftMaster.py")
        let craftTarget = archToCraftTarget(arch)
        let craftCommand = "python3 \(craftMasterPy.path) --config \(craftMasterIni.path) --target \(craftTarget) -c"

        if !fm.fileExists(atPath: craftMasterDir.path) || reconfigureCraft {
            stopwatch.record("KDE Craft Setup")

            if fm.fileExists(atPath: craftMasterDir.path) {
                Log.info("KDE Craft is already cloned.")
            } else {
                Log.info("Cloning KDE Craft...")
                guard await shell("\(gitCloneCommand) \(craftMasterGitUrl) \(craftMasterDir.path)") == 0 else {
                    throw MacCrafterError.gitError("The referenced CraftMaster repository could not be cloned from \(craftMasterGitUrl) to \(craftMasterDir.path)")
                }
            }
            
            Log.info("Configuring required KDE Craft blueprint repositories...")
            stopwatch.record("Craft Blueprints Configuration")

            guard await shell("\(craftCommand) --add-blueprint-repository '\(kdeBlueprintsGitUrl)|\(kdeBlueprintsGitRef)|'") == 0 else {
                throw MacCrafterError.craftError("Error adding KDE blueprint repository.")
            }

            guard await shell("\(craftCommand) --add-blueprint-repository '\(clientBlueprintsGitUrl)|\(clientBlueprintsGitRef)|'") == 0 else {
                throw MacCrafterError.craftError("Error adding Nextcloud Client blueprint repository.")
            }
            
            Log.info("Crafting KDE Craft...")
            stopwatch.record("Craft Crafting")

            guard await shell("\(craftCommand) craft") == 0 else {
                throw MacCrafterError.craftError("Error crafting KDE Craft.")
            }
            
            Log.info("Crafting Nextcloud Desktop Client dependencies...")
            stopwatch.record("Nextcloud Client Dependencies Crafting")

            guard await shell("\(craftCommand) --install-deps \(craftBlueprintName)") == 0 else {
                throw MacCrafterError.craftError("Error installing dependencies.")
            }
        } else {
            Log.info("Skipping KDE Craft configuration because it is already configured and no reconfiguration was requested.")
        }

        var craftOptions = [
            "\(craftBlueprintName).srcDir=\(repoRootURL.path)",
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
        
        if disableAutoUpdater == false {
            Log.info("Configuring Sparkle auto-updater.")
            
            stopwatch.record("Sparkle Configuration")

            let downloadedArchive = try await downloadSparkle()
            let fm = FileManager.default
            
            let sparkleUnarchiveResult = await shell("tar -xvf \(downloadedArchive.path) -C \(buildPath)")
            
            guard fm.fileExists(atPath: "\(buildPath)/Sparkle.framework") || sparkleUnarchiveResult == 0 else {
                throw MacCrafterError.environmentError("Error unpacking sparkle.")
            }
            
            craftOptions.append("\(craftBlueprintName).sparkleLibPath=\(buildPath)/Sparkle.framework")
        }
        
        let clientBuildURL = buildURL
            .appendingPathComponent(craftTarget)
            .appendingPathComponent("build")
            .appendingPathComponent(craftBlueprintName)

        // MARK: Client Crafting

        Log.info("Crafting \(appName) Desktop Client...")
        stopwatch.record("Desktop Client Crafting")

        if fullRebuild {
            if fm.fileExists(atPath: clientBuildURL.path) {
                Log.info("Removing existing client build directory at: \(clientBuildURL.path)")

                do {
                    try fm.removeItem(atPath: clientBuildURL.path)
                } catch {
                    throw MacCrafterError.craftError("Failed to remove existing build directory at: \(clientBuildURL.path)")
                }
            }
        } else {
            // HACK: When building the client we often run into issues with the shell integration
            // component -- particularly the FileProviderExt part. So we wipe out the build
            // artifacts so this part gets build first. Let's first check if we have an existing
            // build in the folder we expect
            let shellIntegrationURL = clientBuildURL
                .appendingPathComponent("work")
                .appendingPathComponent("build")
                .appendingPathComponent("shell_integration")
                .appendingPathComponent("MacOSX")

            if fm.fileExists(atPath: shellIntegrationURL.path) {
                Log.info("Removing existing shell integration build artifacts...")
                do {
                    try fm.removeItem(atPath: shellIntegrationURL.path)
                } catch let error {
                    Log.error("Failed to remove shell integration build directory: \(error)")
                    throw MacCrafterError.craftError("Failed to remove existing shell integration build directory!")
                }
            }
        }
        
        let buildMode = fullRebuild ? "-i" : disableAppBundle ? "--compile" : "--compile --install"
        let offlineMode = offline ? "--offline" : ""
        let allOptionsString = craftOptions.map({ "--options \"\($0)\"" }).joined(separator: " ")

        guard await shell("\(craftCommand) --buildtype \(buildType) \(buildMode) \(offlineMode) \(allOptionsString) \(craftBlueprintName)") == 0 else {
            // Troubleshooting: This can happen because a CraftMaster repository was cloned which does not contain the commit defined in craftmaster.ini of this project due to use of customized forks.
            throw MacCrafterError.craftError("Error crafting Nextcloud Desktop Client.")
        }

        // MARK: Debug Symbols

        let clientAppURL = clientBuildURL
            .appendingPathComponent("image-\(buildType)-master")
            .appendingPathComponent("\(appName).app")

        // When building in dev mode, copy the dSYM bundles for the app extensions from the
        // xcodebuild SYMROOT into Contents/PlugIns/ of the product app bundle alongside their
        // respective .appex bundles.
        //
        // Background: KDE Craft's __internalPostInstallHandleSymbols() deliberately moves every
        // .dSYM bundle out of the main image directory and into a separate -dbg image directory
        // before packaging. This means dSYMs never reach the product app via the normal CMake
        // install() path. Reading directly from the xcodebuild SYMROOT bypasses that filtering.
        //
        // With the dSYMs inside the app bundle under /Applications, Spotlight indexes them and
        // Xcode can find them automatically via UUID lookup when attaching to the extension process,
        // which allows breakpoints in extension source files to be resolved correctly.

        if dev {
            Log.info("Copying extension dSYM bundles into product app bundle for debugging...")

            // XCODE_TARGET_CONFIGURATION in CMakeLists.txt is always "Debug" when NEXTCLOUD_DEV=ON,
            // regardless of the CMake build type passed to Craft.
            let xcodeTargetConfiguration = "Debug"

            let shellIntegrationBuildDir = clientBuildURL
                .appendingPathComponent("work")
                .appendingPathComponent("build")
                .appendingPathComponent("shell_integration")
                .appendingPathComponent("MacOSX")
                .appendingPathComponent(xcodeTargetConfiguration)

            let plugInsDir = clientAppURL
                .appendingPathComponent("Contents")
                .appendingPathComponent("PlugIns")

            guard fm.fileExists(atPath: shellIntegrationBuildDir.path) else {
                Log.info("Shell integration build directory not found, skipping dSYM copy: \(shellIntegrationBuildDir.path)")
                return
            }

            let entries = try fm.contentsOfDirectory(at: shellIntegrationBuildDir, includingPropertiesForKeys: [.isDirectoryKey])
            let dSYMBundles = entries.filter { $0.pathExtension.lowercased() == "dsym" }

            for dSYM in dSYMBundles {
                let destination = plugInsDir.appendingPathComponent(dSYM.lastPathComponent)

                if fm.fileExists(atPath: destination.path) {
                    try fm.removeItem(at: destination)
                }

                try fm.copyItem(at: dSYM, to: destination)
                Log.info("Copied \(dSYM.path) to \(destination.path)")
            }

            if dSYMBundles.isEmpty {
                Log.info("No dSYM bundles found in \(shellIntegrationBuildDir.path)")
            }
        }

        // MARK: Signing

        if let codeSignIdentity {
            Log.info("Signing Nextcloud Desktop Client libraries and frameworks...")
            stopwatch.record("Code Signing")

            let appEntitlements = clientBuildURL
                .appendingPathComponent("work")
                .appendingPathComponent("build")
                .appendingPathComponent("admin")
                .appendingPathComponent("osx")
                .appendingPathComponent("macosx.entitlements")

            let entitlementsDirectory = clientBuildURL
                .appendingPathComponent("work")
                .appendingPathComponent("build")
                .appendingPathComponent("shell_integration")
                .appendingPathComponent("MacOSX")

            let entitlements: [String: URL] = [
                "\(appName).app": appEntitlements,
                "FileProviderExt.appex": entitlementsDirectory.appendingPathComponent("FileProviderExt.entitlements"),
                "FileProviderUIExt.appex": entitlementsDirectory.appendingPathComponent("FileProviderUIExt.entitlements"),
                "FinderSyncExt.appex": entitlementsDirectory.appendingPathComponent("FinderSyncExt.entitlements"),
            ]

            for file in entitlements.values {
                if FileManager.default.fileExists(atPath: file.path) {
                    Log.info("Using entitlement manifest: \(file.path)")
                } else {
                    Log.error("Entitlement manifest does not exist: \(file.path)")
                }
            }

            try await Signer.signMainBundle(at: clientAppURL, codeSignIdentity: codeSignIdentity, entitlements: entitlements)
        }
        
        Log.info("Placing Nextcloud Desktop Client in \(productPath)...")

        if !fm.fileExists(atPath: productPath) {
            try fm.createDirectory(atPath: productPath, withIntermediateDirectories: true, attributes: nil)
        }

        if fm.fileExists(atPath: "\(productPath)/\(appName).app") {
            try fm.removeItem(atPath: "\(productPath)/\(appName).app")
        }

        try fm.copyItem(atPath: clientAppURL.path, toPath: "\(productPath)/\(appName).app")

        // MARK: Packaging

        if package {
            stopwatch.record("Packaging App Bundle")

            try await packageAppBundle(
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
        
        Log.info("Done!")
        Log.info(stopwatch.report())
    }
}
