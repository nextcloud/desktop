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
        let dynamicFrameworksLocation = url
            .appendingPathComponent("Contents")
            .appendingPathComponent("PlugIns")
        
        guard let enumerator = FileManager.default.enumerator(at: dynamicFrameworksLocation, includingPropertiesForKeys: nil) else {
            fatalError("ERROR: Failed to get enumerator for: \(url.path)")
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
        
        var items = try FileManager.default.contentsOfDirectory(at: pluginsLocation, includingPropertiesForKeys: nil)
        
        items.removeAll { item in
            if item.path.hasSuffix(".appex") == false {
                return true
            }
            
            Log.info("Found extension bundle: \(item.path)")
            return false
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
        
        var items = try FileManager.default.contentsOfDirectory(at: frameworksLocation, includingPropertiesForKeys: nil)
        
        items.removeAll { item in
            if item.path.hasSuffix(".framework") == false {
                return true
            }
            
            Log.info("Found framework bundle: \(item.path)")
            return false
        }
        
        return items
    }
    
    private static func verify(at location: URL) async {
        Log.info("Verifying: \(location.path)")
        await shell("codesign --verify --deep --strict --verbose=2 \"\(location.path)\"")
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
        
        let frameworksInsideMainBundle = try findFrameworks(at: location)

        try await withThrowingTaskGroup(of: Void.self) { group in
            for frameworkInMainBundle in frameworksInsideMainBundle {
                group.addTask {
                    await sign(at: frameworkInMainBundle, with: codeSignIdentity, entitlements: nil)
                }
            }

            try await group.waitForAll()
        }

        let dynamicLibraries = try findDynamicLibraries(at: location)

        try await withThrowingTaskGroup(of: Void.self) { group in
            for dynamicLibrary in dynamicLibraries {
                group.addTask {
                    await sign(at: dynamicLibrary, with: codeSignIdentity, entitlements: nil)
                }
            }

            try await group.waitForAll()
        }

        guard let mainAppEntitlements = entitlements[location.lastPathComponent] else {
            throw MacCrafterError.signing("No entitlements provided for: \(location.path)")
        }

        await sign(at: location, with: codeSignIdentity, entitlements: mainAppEntitlements)
        await verify(at: location)
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

        var command = [
            "codesign",
            location.path,
            "--timestamp",
            "--verbose=4",
            "--preserve-metadata=entitlements",
            "--force",
            "--sign=\"\(codeSignIdentity)\""
        ]

        if let entitlements {
            command.append(" --entitlements=\"\(entitlements.path)\"")
        }
        
        await shell(command.joined(separator: " "))
    }
}
