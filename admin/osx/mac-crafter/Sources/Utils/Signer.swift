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
            
            print("Found extension bundle: \(item.path)")
            return false
        }
        
        return items
    }

    ///
    /// Extract the associated entitlements of a bundle into an XML file.
    ///
    /// - Parameters:
    ///     - location: An app or extension bundle.
    ///
    /// - Returns: The location of the entitlements file for the given source bundle.
    ///
    private static func saveEntitlements(of location: URL) async throws -> URL {
        let destination = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString).appendingPathExtension("entitlements")
        print("Saving entitlements of \"\(location.path)\" to \"\(destination.path)\"…")

        guard await shell("codesign -d --entitlements '\(destination.path)' --xml '\(location.path)'") == 0 else {
            throw MacCrafterError.signing("Failed to save entitlements of \"\(location.path)\" to \"\(destination.path)\"!")
        }

        guard let data = try? Data(contentsOf: destination) else {
            throw MacCrafterError.signing("Failed to read data of entitlements file!")
        }

        guard let string = String(data: data, encoding: .utf8) else {
            throw MacCrafterError.signing("Failed to convert entitlements data to string!")
        }

        print("Saved entitlements: \(string)")

        return destination
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
            
            print("Found framework bundle: \(item.path)")
            return false
        }
        
        return items
    }
    
    private static func verify(at location: URL) async {
        print("Verifying: \(location.path)")
        await shell("codesign --verify --deep --strict --verbose=2 \"\(location.path)\"")
    }

    // MARK: - Public
    
    ///
    /// Entry point for signing a whole desktop client app bundle.
    ///
    static func signMainBundle(at location: URL, codeSignIdentity: String, developerBuild: Bool) async throws {
        let extensions = try findExtensions(at: location)
        
        for extensionInMainBundle in extensions {
            let frameworksInsideExtension = try findFrameworks(at: extensionInMainBundle)
            
            for frameworkInExtension in frameworksInsideExtension {
                await sign(at: frameworkInExtension, with: codeSignIdentity, entitlements: nil)
            }

            let extensionEntitlements = try await saveEntitlements(of: extensionInMainBundle)
            await sign(at: extensionInMainBundle, with: codeSignIdentity, entitlements: extensionEntitlements)
        }
        
        let frameworksInsideMainBundle = try findFrameworks(at: location)
        
        for frameworkInMainBundle in frameworksInsideMainBundle {
            await sign(at: frameworkInMainBundle, with: codeSignIdentity, entitlements: nil)
        }
        
        let dynamicLibraries = try findDynamicLibraries(at: location)
        
        for dynamicLibrary in dynamicLibraries {
            await sign(at: dynamicLibrary, with: codeSignIdentity, entitlements: nil)
        }

        let mainAppEntitlements = try await saveEntitlements(of: location)
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
        print("Signing: \(location.path)")

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
