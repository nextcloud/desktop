//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Signing features.
///
protocol Signing {
    static func sign(at location: URL, with codeSignIdentity: String, entitlements: URL?) async
}

///
/// Used as a namespace for stateless signing methods.
///
enum Signer: Signing {
    
    // MARK: - Private
    
    private static func findDynamicLibraries(at url: URL) throws -> [URL] {
        Log.info("Looking for dynamic libraries in \(url.path)")

        guard let enumerator = FileManager.default.enumerator(at: url, includingPropertiesForKeys: nil) else {
            throw MacCrafterError.environmentError("Failed to get enumerator for: \(url.path)")
        }
        
        let dynamicLibaries: [URL] = enumerator.compactMap { element in
            guard let candidate = element as? URL else {
                return nil
            }
            
            guard candidate.path.contains(".appex/") == false else {
                return nil
            }
            
            guard candidate.pathExtension == "dylib" else {
                return nil
            }

            Log.info("Found dynamic library: \(candidate.path)")
            return candidate
        }

        return dynamicLibaries
    }
    
    ///
    /// Find all extension bundles in the bundle at the given location.
    ///
    /// This assumes the internal structure of the bundle at the given location to have `Contents/PlugIns`.
    ///
    private static func findExtensions(at url: URL) throws -> [URL] {
        let pluginsLocation = url
            .appendingPathComponent("Contents")
            .appendingPathComponent("PlugIns")

        Log.info("Looking for extensions in \(pluginsLocation.path)")
        var items = try FileManager.default.contentsOfDirectory(at: pluginsLocation, includingPropertiesForKeys: nil)
        
        items.removeAll { item in
            if item.path.hasSuffix(".appex") {
                Log.info("Found extension bundle: \(item.path)")
                return false
            } else {
                Log.info("Skipping item that is not an extension bundle: \(item.path)")
                return true
            }
        }
        
        return items
    }

    ///
    /// Find all framework bundles in the bundle at the given location.
    ///
    /// This assumes the internal structure of the bundle at the given location to have `Contents/Frameworks`.
    ///
    private static func findFrameworks(at url: URL) throws -> [URL] {
        let frameworksLocation = url
            .appendingPathComponent("Contents")
            .appendingPathComponent("Frameworks")

        Log.info("Looking for frameworks in \(frameworksLocation.path)")
        var items = try FileManager.default.contentsOfDirectory(at: frameworksLocation, includingPropertiesForKeys: nil)
        
        items.removeAll { item in
            if ["dylib", "framework"].contains(item.pathExtension) {
                Log.info("Found item to sign: \(item.path)")
                return false
            } else {
                Log.info("Skipping item due to invalid path extension: \(item.path)")
                return true
            }
        }
        
        return items
    }

    ///
    /// Check whether the given file is an native executable binary or not.
    ///
    private static func isExecutable(_ file: URL) async throws -> Bool {
        let outPipe = Pipe()
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/bin/zsh")
        task.arguments = ["-c", "file \"\(file.path)\""]
        task.standardOutput = outPipe
        task.standardError = Pipe()

        try task.run()
        task.waitUntilExit()

        let outputData = outPipe.fileHandleForReading.readDataToEndOfFile()
        let output = String(data: outputData, encoding: .utf8) ?? ""

        return output.contains("Mach-O 64-bit executable")
    }

    ///
    /// Find and sign the Qt web engine helper app inside the QtWebEngineCore framework.
    ///
    /// This needs explicit treatment because codesign does not automatically sign it when signing the upstream framework bundle.
    ///
    private static func signQtWebEngineProcessApp(in bundle: URL, with codeSignIdentity: String) async {
        let location = bundle
            .appendingPathComponent("Contents")
            .appendingPathComponent("Frameworks")
            .appendingPathComponent("QtWebEngineCore.framework")
            .appendingPathComponent("Versions")
            .appendingPathComponent("A")
            .appendingPathComponent("Helpers")
            .appendingPathComponent("QtWebEngineProcess.app")

        let entitlements = location
            .appendingPathComponent("Contents")
            .appendingPathComponent("Resources")
            .appendingPathComponent("QtWebEngineProcess")
            .appendingPathExtension("entitlements")

        await sign(at: location, with: codeSignIdentity, entitlements: entitlements)
    }

    ///
    /// Find and sign the Sparkle downloader inside the Sparkle framework.
    ///
    /// This needs explicit treatment because codesign does not automatically sign it when signing the upstream framework bundle.
    ///
    private static func signSparkleDownloader(in bundle: URL, with codeSignIdentity: String) async {
        let location = bundle
            .appendingPathComponent("Contents")
            .appendingPathComponent("Frameworks")
            .appendingPathComponent("Sparkle.framework")
            .appendingPathComponent("Versions")
            .appendingPathComponent("B")
            .appendingPathComponent("XPCServices")
            .appendingPathComponent("Downloader")
            .appendingPathExtension("xpc")

        await sign(at: location, with: codeSignIdentity, entitlements: nil)
    }

    ///
    /// Find and sign the Sparkle Installer inside the Sparkle framework.
    ///
    /// This needs explicit treatment because codesign does not automatically sign it when signing the upstream framework bundle.
    ///
    private static func signSparkleInstaller(in bundle: URL, with codeSignIdentity: String) async {
        let location = bundle
            .appendingPathComponent("Contents")
            .appendingPathComponent("Frameworks")
            .appendingPathComponent("Sparkle.framework")
            .appendingPathComponent("Versions")
            .appendingPathComponent("B")
            .appendingPathComponent("XPCServices")
            .appendingPathComponent("Installer")
            .appendingPathExtension("xpc")

        await sign(at: location, with: codeSignIdentity, entitlements: nil)
    }
    
    ///
    /// Find and sign the Sparkle autoupdate inside the Sparkle framework.
    ///
    /// This needs explicit treatment because codesign does not automatically sign it when signing the upstream framework bundle.
    ///
    private static func signSparkleAutoupdate(in bundle: URL, with codeSignIdentity: String) async {
        let location = bundle
            .appendingPathComponent("Contents")
            .appendingPathComponent("Frameworks")
            .appendingPathComponent("Sparkle.framework")
            .appendingPathComponent("Versions")
            .appendingPathComponent("B")
            .appendingPathComponent("Autoupdate")

        await sign(at: location, with: codeSignIdentity, entitlements: nil)
    }

    ///
    /// Find and sign the Sparkle updater app inside the Sparkle framework.
    ///
    /// This needs explicit treatment because codesign does not automatically sign it when signing the upstream framework bundle.
    ///
    private static func signSparkleUpdaterApp(in bundle: URL, with codeSignIdentity: String) async {
        let location = bundle
            .appendingPathComponent("Contents")
            .appendingPathComponent("Frameworks")
            .appendingPathComponent("Sparkle.framework")
            .appendingPathComponent("Versions")
            .appendingPathComponent("B")
            .appendingPathComponent("Updater")
            .appendingPathExtension("app")

        await sign(at: location, with: codeSignIdentity, entitlements: nil)
    }

    ///
    /// There may be additional executables in the binaries directory which also need to be signed.
    ///
    private static func signAdditionalBinaries(in bundle: URL, with codeSignIdentity: String) async throws {
        let location = bundle
            .appendingPathComponent("Contents")
            .appendingPathComponent("MacOS")

        let candidates = try FileManager.default.contentsOfDirectory(at: location, includingPropertiesForKeys: nil)

        for candidate in candidates {
            if try await isExecutable(candidate) {
                await sign(at: candidate, with: codeSignIdentity, entitlements: nil)
            }
        }
    }

    private static func verify(at location: URL) async throws {
        Log.info("Verifying: \(location.path)")
        let code = await shell("codesign --verify --deep --strict --verbose=2 \"\(location.path)\"")

        if code > 0 {
            throw MacCrafterError.signing("Signing verification failed because the codesign command terminated with code \(code)")
        }
    }

    // MARK: - Public
    
    ///
    /// Entry point for signing a whole desktop client app bundle.
    ///
    static func signMainBundle(at location: URL, codeSignIdentity: String, entitlements: [String: URL]) async throws {
        let extensions = try findExtensions(at: location)
        
        for extensionInMainBundle in extensions {
            let frameworksInsideExtension = try findFrameworks(at: extensionInMainBundle)

            try await withThrowingTaskGroup(of: Void.self) { group in
                for frameworkInExtension in frameworksInsideExtension {
                    group.addTask {
                        await sign(at: frameworkInExtension, with: codeSignIdentity, entitlements: nil)
                    }
                }

                try await group.waitForAll()
            }

            guard let extensionEntitlements = entitlements[extensionInMainBundle.lastPathComponent] else {
                throw MacCrafterError.signing("No entitlements provided for: \(extensionInMainBundle.path)")
            }

            await sign(at: extensionInMainBundle, with: codeSignIdentity, entitlements: extensionEntitlements)
        }

        await signQtWebEngineProcessApp(in: location, with: codeSignIdentity)
        await signSparkleDownloader(in: location, with: codeSignIdentity)
        await signSparkleUpdaterApp(in: location, with: codeSignIdentity)
        await signSparkleInstaller(in: location, with: codeSignIdentity)
        await signSparkleAutoupdate(in: location, with: codeSignIdentity)

        let frameworksInsideMainBundle = try findFrameworks(at: location)

        try await withThrowingTaskGroup(of: Void.self) { group in
            for frameworkInMainBundle in frameworksInsideMainBundle {
                group.addTask {
                    await sign(at: frameworkInMainBundle, with: codeSignIdentity, entitlements: nil)
                }
            }

            try await group.waitForAll()
        }

        var dynamicLibraries = [URL]()

        let binariesLocation = location
            .appendingPathComponent("Contents")
            .appendingPathComponent("MacOS")

        dynamicLibraries.append(contentsOf: try findDynamicLibraries(at: binariesLocation))

        let pluginsLocation = location
            .appendingPathComponent("Contents")
            .appendingPathComponent("PlugIns")

        dynamicLibraries.append(contentsOf: try findDynamicLibraries(at: pluginsLocation))

        for dynamicLibrary in dynamicLibraries {
            await sign(at: dynamicLibrary, with: codeSignIdentity, entitlements: nil)
        }

        try await signAdditionalBinaries(in: location, with: codeSignIdentity)

        guard let mainAppEntitlements = entitlements[location.lastPathComponent] else {
            throw MacCrafterError.signing("No entitlements provided for: \(location.path)")
        }

        await sign(at: location, with: codeSignIdentity, entitlements: mainAppEntitlements)
        try await verify(at: location)
    }
    
    ///
    /// Shell out to `codesign`.
    ///
    /// - Parameters:
    ///     - location: The top-level item to sign. Might be a bundle or file.
    ///     - codeSignIdentity: The common name of the certificate available in the keychain to use for signing.
    ///
    static func sign(at location: URL, with codeSignIdentity: String, entitlements: URL?) async {
        Log.info("Signing: \(location.path)")

        var commandComponents = [
            "codesign",
            location.path,
            "--timestamp",
            "--verbose=4",
            "--force",
            "--options=runtime",
            "--sign=\"\(codeSignIdentity)\""
        ]

        if let entitlements {
            commandComponents.append(" --entitlements=\"\(entitlements.path)\"")
        } else {
            commandComponents.append("--preserve-metadata=entitlements")
        }

        let command = commandComponents.joined(separator: " ")
        await shell(command)
    }
}
