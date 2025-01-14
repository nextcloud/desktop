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

import Foundation

fileprivate let defaultCodesignOptions = "--timestamp --force --preserve-metadata=entitlements --verbose=4 --options runtime"

enum CodeSigningError: Error {
    case failedToCodeSign(String)
}

enum AppBundleSigningError: Error {
    case doesNotExist(String)
    case couldNotEnumerate(String)
}

func isLibrary(_ path: String) -> Bool {
    path.hasSuffix(".dylib") || path.hasSuffix(".framework")
}

func isAppExtension(_ path: String) -> Bool {
    path.hasSuffix(".appex")
}

func isExecutable(_ path: String) throws -> Bool {
    let outPipe = Pipe()
    let errPipe = Pipe()
    let task = Process()
    task.standardOutput = outPipe
    task.standardError = errPipe

    let command = "file \"\(path)\""
    guard run("/bin/zsh", ["-c", command], task: task) == 0 else {
        throw CodeSigningError.failedToCodeSign("Failed to determine if \(path) is an executable.")
    }

    let outputFileHandle = outPipe.fileHandleForReading
    let outputData = outputFileHandle.readDataToEndOfFile()
    try outputFileHandle.close()
    let output = String(data: outputData, encoding: .utf8) ?? ""
    return output.contains("Mach-O 64-bit executable")
}

func codesign(identity: String, path: String, options: String = defaultCodesignOptions) throws {
    print("Code-signing \(path)...")
    let command = "codesign -s \"\(identity)\" \(options) \"\(path)\""
    guard shell(command) == 0 else {
        throw CodeSigningError.failedToCodeSign("Failed to code-sign \(path).")
    }
}

func recursivelyCodesign(
    path: String,
    identity: String,
    options: String = defaultCodesignOptions,
    skip: [String] = []
) throws {
    let fm = FileManager.default
    guard fm.fileExists(atPath: path) else {
        throw AppBundleSigningError.doesNotExist("Item at \(path) does not exist.")
    }

    guard let pathEnumerator = fm.enumerator(atPath: path) else {
        throw AppBundleSigningError.couldNotEnumerate(
            "Failed to enumerate directory at \(path)."
        )
    }

    for case let enumeratedItem as String in pathEnumerator {
        let enumeratedItemPath = "\(path)/\(enumeratedItem)"
        guard !skip.contains(enumeratedItemPath) else {
            print("Skipping \(enumeratedItemPath)...")
            continue
        }
        let isExecutableFile = try isExecutable(enumeratedItemPath)
        guard isLibrary(enumeratedItem) || isAppExtension(enumeratedItem) || isExecutableFile else {
            continue
        }
        try codesign(identity: identity, path: enumeratedItemPath, options: options)
    }
}

func saveCodesignEntitlements(target: String, path: String) throws {
    let command = "codesign -d --entitlements \"\(path)\" --xml \"\(target)\""
    guard shell(command) == 0 else {
        throw CodeSigningError.failedToCodeSign("Failed to save entitlements for \(target).")
    }
}

func codesignClientAppBundle(
    at clientAppDir: String, withCodeSignIdentity codeSignIdentity: String
) throws {
    print("Code-signing Nextcloud Desktop Client libraries, frameworks and plugins...")

    let clientContentsDir = "\(clientAppDir)/Contents"
    let frameworksPath = "\(clientContentsDir)/Frameworks"
    let pluginsPath = "\(clientContentsDir)/PlugIns"

    try recursivelyCodesign(path: frameworksPath, identity: codeSignIdentity)
    try recursivelyCodesign(path: pluginsPath, identity: codeSignIdentity)
    try recursivelyCodesign(path: "\(clientContentsDir)/Resources", identity: codeSignIdentity)

    print("Code-signing QtWebEngineProcess...")
    let qtWebEngineProcessPath =
        "\(frameworksPath)/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"
    try codesign(identity: codeSignIdentity,
                 path: qtWebEngineProcessPath,
                 options: "--timestamp --force --verbose=4 --options runtime --deep --entitlements \"\(qtWebEngineProcessPath)/Contents/Resources/QtWebEngineProcess.entitlements\"")

    print("Code-signing QtWebEngine...")
    try codesign(identity: codeSignIdentity, path: "\(frameworksPath)/QtWebEngineCore.framework")

    // Time to fix notarisation issues.
    // Multiple components of the app will now have the get-task-allow entitlements.
    // We need to strip these out manually.

    let sparkleFrameworkPath = "\(frameworksPath)/Sparkle.framework"
    if FileManager.default.fileExists(atPath: sparkleFrameworkPath) {
        print("Code-signing Sparkle...")
        try codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/XPCServices/Installer.xpc",
            options: "-f -o runtime"
        )
        try codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/XPCServices/Downloader.xpc",
            options: "-f -o runtime --preserve-metadata=entitlements"
        )
        try codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/Autoupdate",
            options: "-f -o runtime"
        )
        try codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/Updater.app",
            options: "-f -o runtime"
        )
        try codesign(
            identity: codeSignIdentity, path: sparkleFrameworkPath, options: "-f -o runtime"
        )
    } else {
        print("Build does not have Sparkle, skipping.")
    }

    print("Code-signing app extensions (removing get-task-allow entitlements)...")
    let fm = FileManager.default
    let appExtensionPaths =
        try fm.contentsOfDirectory(atPath: pluginsPath).filter(isAppExtension)
    for appExtension in appExtensionPaths {
        let appExtensionPath = "\(pluginsPath)/\(appExtension)"
        let tmpEntitlementXmlPath =
            fm.temporaryDirectory.appendingPathComponent(UUID().uuidString).path.appending(".xml")
        try saveCodesignEntitlements(target: appExtensionPath, path: tmpEntitlementXmlPath)
        // Strip the get-task-allow entitlement from the XML entitlements file
        let xmlEntitlements = try String(contentsOfFile: tmpEntitlementXmlPath)
        let entitlementKeyValuePair = "<key>com.apple.security.get-task-allow</key><true/>"
        let strippedEntitlements =
            xmlEntitlements.replacingOccurrences(of: entitlementKeyValuePair, with: "")
        try strippedEntitlements.write(toFile: tmpEntitlementXmlPath,
                                       atomically: true,
                                       encoding: .utf8)
        try codesign(identity: codeSignIdentity,
                     path: appExtensionPath,
                     options: "--timestamp --force --verbose=4 --options runtime --deep --entitlements \"\(tmpEntitlementXmlPath)\"")
    }

    // Now we do the final codesign bit
    let binariesDir = "\(clientContentsDir)/MacOS"
    print("Code-signing Nextcloud Desktop Client binaries...")

    guard let appName = clientAppDir.components(separatedBy: "/").last, clientAppDir.hasSuffix(".app") else {
        throw AppBundleSigningError.couldNotEnumerate("Failed to determine main executable name.")
    }

    // Sign the main executable last
    let mainExecutableName = String(appName.dropLast(".app".count))
    let mainExecutablePath = "\(binariesDir)/\(mainExecutableName)"
    try recursivelyCodesign(path: binariesDir, identity: codeSignIdentity, skip: [mainExecutablePath])
    try codesign(identity: codeSignIdentity, path: mainExecutablePath)
}
