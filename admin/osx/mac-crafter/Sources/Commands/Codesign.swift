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
    
    @Option(name: [.short, .long], help: "Entitlements to apply to the app bundle.")
    var entitlementsPath: String?
    
    mutating func run() async throws {
        let absolutePath = appBundlePath.hasPrefix("/") ? appBundlePath : "\(FileManager.default.currentDirectoryPath)/\(appBundlePath)"
        let url = URL(fileURLWithPath: absolutePath)
        try await signMainBundle(at: url)
    }
    
    private func findDynamicLibraries(at url: URL) throws -> [URL] {
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
    private func findExtensions(at url: URL) throws -> [URL] {
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
    /// Find all framework bundles in the bundle at the given location.
    ///
    /// This assumes the internal structure of the bundle at the given location to have `Contents/Frameworks`.
    ///
    private func findFrameworks(at url: URL) throws -> [URL] {
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
    
    ///
    /// Entry point for signing a whole desktop client app bundle.
    ///
    private func signMainBundle(at location: URL) async throws {
        let extensions = try findExtensions(at: location)
        
        for extensionInMainBundle in extensions {
            let frameworksInsideExtension = try findFrameworks(at: extensionInMainBundle)
            
            for frameworkInExtension in frameworksInsideExtension {
                await sign(at: frameworkInExtension)
            }
            
            await sign(at: extensionInMainBundle)
        }
        
        let frameworksInsideMainBundle = try findFrameworks(at: location)
        
        for frameworkInMainBundle in frameworksInsideMainBundle {
            await sign(at: frameworkInMainBundle)
        }
        
        let dynamicLibraries = try findDynamicLibraries(at: location)
        
        for dynamicLibrary in dynamicLibraries {
            await sign(at: dynamicLibrary)
        }
        
        await sign(at: location)
        await verify(at: location)
    }
    
    ///
    /// Run `codesign` with the given path.
    ///
    private func sign(at location: URL) async {
        print("Signing: \(location.path)")
        await shell("codesign --force --sign \"\(codeSignIdentity)\" \"\(location.path)\"")
    }
    
    private func verify(at location: URL) async {
        print("Verifying: \(location.path)")
        await shell("codesign --verify --deep --strict --verbose=2 \"\(location.path)\"")
    }
}
