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
/// Team identifier verification helpers.
///
enum TeamIdentifierVerifier {
    static func teamIdentifier(from codesignOutput: String) -> String? {
        let prefix = "TeamIdentifier="

        guard let line = codesignOutput.split(whereSeparator: \.isNewline).first(where: { $0.hasPrefix(prefix) }) else {
            return nil
        }

        let teamIdentifier = line.dropFirst(prefix.count).trimmingCharacters(in: .whitespacesAndNewlines)
        return teamIdentifier == "not set" || teamIdentifier.isEmpty ? nil : teamIdentifier
    }

    static func validationError(for components: [(location: String, teamIdentifier: String?)]) -> String? {
        guard let firstComponent = components.first else {
            return "Signing verification failed because no signed code components were found"
        }

        guard let expectedTeamIdentifier = firstComponent.teamIdentifier else {
            return "Signing verification failed because \(firstComponent.location) has no TeamIdentifier"
        }

        for component in components.dropFirst() {
            guard let teamIdentifier = component.teamIdentifier else {
                return "Signing verification failed because \(component.location) has no TeamIdentifier"
            }

            guard teamIdentifier == expectedTeamIdentifier else {
                return "Signing verification failed because \(component.location) has TeamIdentifier \(teamIdentifier), expected \(expectedTeamIdentifier) from \(firstComponent.location)"
            }
        }

        return nil
    }
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

        guard FileManager.default.fileExists(atPath: pluginsLocation.path) else {
            Log.info("No PlugIns directory found, skipping extension signing")
            return []
        }

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
    /// Find all Service Management login item bundles in the bundle at the given location.
    ///
    /// This assumes the internal structure of the bundle at the given location to have
    /// `Contents/Library/LoginItems`.
    ///
    /// Login items have to be discovered and signed explicitly. Signing the outer app bundle
    /// does not reach them, and the outer sign is deliberately not `--deep`, so without this
    /// the FinderSync broker would ship unsigned — which notarization would reject and which
    /// would in any case make the sandboxed peers refuse to talk to it.
    ///
    private static func findLoginItems(at url: URL) throws -> [URL] {
        let loginItemsLocation = url
            .appendingPathComponent("Contents")
            .appendingPathComponent("Library")
            .appendingPathComponent("LoginItems")

        guard FileManager.default.fileExists(atPath: loginItemsLocation.path) else {
            Log.info("No LoginItems directory found, skipping login item signing")
            return []
        }

        Log.info("Looking for login items in \(loginItemsLocation.path)")
        var items = try FileManager.default.contentsOfDirectory(at: loginItemsLocation, includingPropertiesForKeys: nil)

        items.removeAll { item in
            if item.pathExtension == "app" {
                Log.info("Found login item bundle: \(item.path)")
                return false
            } else {
                Log.info("Skipping item that is not a login item bundle: \(item.path)")
                return true
            }
        }

        return items
    }

    ///
    /// Fail the build if a login item's `CFBundleIdentifier` does not equal its wrapper filename.
    ///
    /// A Service Management login item is addressed by bundle identifier, and its wrapper has to
    /// be named after that identifier. When the two disagree, `SMAppService` cannot resolve the
    /// item: both `-status` and `-registerAndReturnError:` fail with EINVAL, the client logs
    /// "Unable to find service status … error: 22", no launchd job is ever created, and any
    /// extension then loops forever against a Mach service that does not exist.
    ///
    /// None of that is visible at build time.
    /// Checking the built bundle is the only place the disagreement is observable.
    ///
    private static func assertLoginItemIdentifierMatchesFilename(_ loginItem: URL) throws {
        let plist = loginItem
            .appendingPathComponent("Contents")
            .appendingPathComponent("Info.plist")

        guard let contents = NSDictionary(contentsOf: plist),
              let identifier = contents["CFBundleIdentifier"] as? String
        else {
            throw MacCrafterError.signing("Could not read CFBundleIdentifier from: \(plist.path)")
        }

        let expected = loginItem.deletingPathExtension().lastPathComponent

        guard identifier == expected else {
            throw MacCrafterError.signing("""
                Login item bundle identifier does not match its wrapper filename, so \
                SMAppService will not be able to register it: \
                CFBundleIdentifier is "\(identifier)" but the wrapper is "\(expected).app". \
                Check that the login item's Info.plist uses $(PRODUCT_BUNDLE_IDENTIFIER) rather \
                than spelling the identifier out.
                """)
        }

        Log.info("Login item identifier matches its wrapper filename: \(identifier)")
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

        guard FileManager.default.fileExists(atPath: frameworksLocation.path) else {
            Log.info("No Frameworks directory found, skipping")
            return []
        }

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

    ///
    /// Find code objects whose signatures must belong to the app's signing team.
    ///
    private static func findCodeComponents(at url: URL) throws -> [URL] {
        let codeBundleExtensions = ["app", "appex", "framework", "xpc"]
        let codeSearchDirectories = ["/Contents/MacOS/", "/Contents/Frameworks/", "/Contents/PlugIns/"]
        var components = [url]

        guard let enumerator = FileManager.default.enumerator(
            at: url,
            includingPropertiesForKeys: [.isRegularFileKey]
        ) else {
            throw MacCrafterError.environmentError("Failed to get enumerator for: \(url.path)")
        }

        for case let candidate as URL in enumerator {
            let pathExtension = candidate.pathExtension.lowercased()

            if codeBundleExtensions.contains(pathExtension) || pathExtension == "dylib" {
                components.append(candidate)
                continue
            }

            guard codeSearchDirectories.contains(where: candidate.path.contains),
                  try candidate.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile == true,
                  isMachO(candidate)
            else {
                continue
            }

            components.append(candidate)
        }

        return components.sorted { $0.path < $1.path }
    }

    ///
    /// Check whether a file contains a Mach-O code object.
    ///
    private static func isMachO(_ file: URL) -> Bool {
        let task = Process()
        let outputPipe = Pipe()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/file")
        task.arguments = ["-b", file.path]
        task.standardOutput = outputPipe
        task.standardError = Pipe()

        do {
            try task.run()
        } catch {
            return false
        }

        let output = String(data: outputPipe.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        task.waitUntilExit()
        return task.terminationStatus == 0 && output.contains("Mach-O")
    }

    ///
    /// Read the code-signing metadata for a code object.
    ///
    private static func codesignDetails(at location: URL) throws -> String {
        let task = Process()
        let standardOutput = Pipe()
        let standardError = Pipe()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/codesign")
        task.arguments = ["--display", "--verbose=4", location.path]
        task.standardOutput = standardOutput
        task.standardError = standardError

        do {
            try task.run()
        } catch {
            throw MacCrafterError.signing("Unable to inspect the code signature of \(location.path): \(error.localizedDescription)")
        }

        let outputData = standardOutput.fileHandleForReading.readDataToEndOfFile()
        let errorData = standardError.fileHandleForReading.readDataToEndOfFile()
        task.waitUntilExit()

        guard task.terminationStatus == 0 else {
            let errorOutput = String(data: errorData, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines) ?? "unknown codesign error"
            throw MacCrafterError.signing("Unable to inspect the code signature of \(location.path): \(errorOutput)")
        }

        return [outputData, errorData]
            .compactMap { String(data: $0, encoding: .utf8) }
            .joined(separator: "\n")
    }

    private static func verifyTeamIdentifiers(at location: URL) throws {
        let components = try findCodeComponents(at: location)
        let signatures = try components.map { component in
            (
                location: component.path,
                teamIdentifier: TeamIdentifierVerifier.teamIdentifier(from: try codesignDetails(at: component))
            )
        }

        if let error = TeamIdentifierVerifier.validationError(for: signatures) {
            throw MacCrafterError.signing(error)
        }

        Log.info("Verified matching TeamIdentifier for \(signatures.count) code components")
    }

    private static func verify(at location: URL) async throws {
        Log.info("Verifying: \(location.path)")
        let code = await shell("codesign --verify --deep --strict --verbose=2 \"\(location.path)\"")

        if code > 0 {
            throw MacCrafterError.signing("Signing verification failed because the codesign command terminated with code \(code)")
        }

        try verifyTeamIdentifiers(at: location)
    }

    // MARK: - Public
    
    ///
    /// Entry point for signing a whole desktop client app bundle.
    ///
    static func signMainBundle(
        at location: URL,
        codeSignIdentity: String,
        entitlements: [String: URL]
    ) async throws {
        // Signing is inside-out: nested code first, the containing bundle last. Login items come
        // before the app for that reason, and the outer sign is deliberately not --deep.
        let loginItems = try findLoginItems(at: location)

        for loginItem in loginItems {
            guard let loginItemEntitlements = entitlements[loginItem.lastPathComponent] else {
                throw MacCrafterError.signing("No entitlements provided for: \(loginItem.path)")
            }

            try assertLoginItemIdentifierMatchesFilename(loginItem)

            await sign(at: loginItem, with: codeSignIdentity, entitlements: loginItemEntitlements)
        }

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
