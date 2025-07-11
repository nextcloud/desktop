/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

func isExecutable(_ path: String) async throws -> Bool {
    let outPipe = Pipe()
    let errPipe = Pipe()
    let task = Process()
    task.standardOutput = outPipe
    task.standardError = errPipe

    let command = "file \"\(path)\""
    guard await run("/bin/zsh", ["-c", command], task: task) == 0 else {
        throw CodeSigningError.failedToCodeSign("Failed to determine if \(path) is an executable.")
    }

    let outputFileHandle = outPipe.fileHandleForReading
    let outputData = outputFileHandle.readDataToEndOfFile()
    try outputFileHandle.close()
    let output = String(data: outputData, encoding: .utf8) ?? ""
    return output.contains("Mach-O 64-bit executable")
}

func codesign(identity: String, path: String, options: String = defaultCodesignOptions) async throws {
    print("Code-signing \(path)...")
    let command = "codesign -s \"\(identity)\" \(options) \"\(path)\""
    for _ in 1...5 {
        guard await shell(command) == 0 else {
            print("Code-signing failed, retrying ...")
            continue
        }

        // code signing was successful
        return
    }

    throw CodeSigningError.failedToCodeSign("Failed to code-sign \(path).")
}

func recursivelyCodesign(
    path: String,
    identity: String,
    options: String = defaultCodesignOptions,
    skip: [String] = []
) async throws {
    let fm = FileManager.default
    guard fm.fileExists(atPath: path) else {
        throw AppBundleSigningError.doesNotExist("Item at \(path) does not exist.")
    }

    let enumeratedItems: [String]
    do {
        enumeratedItems = try fm.subpathsOfDirectory(atPath: path)
    } catch {
        throw AppBundleSigningError.couldNotEnumerate(
            "Failed to enumerate directory at \(path)."
        )
    }

    for enumeratedItem in enumeratedItems {
        let enumeratedItemPath = "\(path)/\(enumeratedItem)"
        guard !skip.contains(enumeratedItemPath) else {
            print("Skipping \(enumeratedItemPath)...")
            continue
        }
        let isExecutableFile = try await isExecutable(enumeratedItemPath)
        guard isLibrary(enumeratedItem) || isAppExtension(enumeratedItem) || isExecutableFile else {
            continue
        }
        try await codesign(identity: identity, path: enumeratedItemPath, options: options)
    }
}

func saveCodesignEntitlements(target: String, path: String) async throws {
    let command = "codesign -d --entitlements \"\(path)\" --xml \"\(target)\""
    guard await shell(command) == 0 else {
        throw CodeSigningError.failedToCodeSign("Failed to save entitlements for \(target).")
    }
}

func codesignClientAppBundle(
    at clientAppDir: String,
    withCodeSignIdentity codeSignIdentity: String,
    usingEntitlements entitlementsPath: String? = nil
) async throws {
    print("Code-signing Nextcloud Desktop Client libraries, frameworks and plugins...")

    let clientContentsDir = "\(clientAppDir)/Contents"
    let frameworksPath = "\(clientContentsDir)/Frameworks"
    let pluginsPath = "\(clientContentsDir)/PlugIns"

    try await recursivelyCodesign(path: frameworksPath, identity: codeSignIdentity)
    try await recursivelyCodesign(path: pluginsPath, identity: codeSignIdentity)
    try await recursivelyCodesign(path: "\(clientContentsDir)/Resources", identity: codeSignIdentity)

    print("Code-signing QtWebEngineProcess...")
    let qtWebEngineProcessPath =
        "\(frameworksPath)/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"
    try await codesign(identity: codeSignIdentity,
                 path: qtWebEngineProcessPath,
                 options: "--timestamp --force --verbose=4 --options runtime --deep --entitlements \"\(qtWebEngineProcessPath)/Contents/Resources/QtWebEngineProcess.entitlements\"")

    print("Code-signing QtWebEngine...")
    try await codesign(identity: codeSignIdentity, path: "\(frameworksPath)/QtWebEngineCore.framework")

    // Time to fix notarisation issues.
    // Multiple components of the app will now have the get-task-allow entitlements.
    // We need to strip these out manually.

    let sparkleFrameworkPath = "\(frameworksPath)/Sparkle.framework"
    if FileManager.default.fileExists(atPath: sparkleFrameworkPath) {
        print("Code-signing Sparkle...")
        try await codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/XPCServices/Installer.xpc",
            options: "-f -o runtime"
        )
        try await codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/XPCServices/Downloader.xpc",
            options: "-f -o runtime --preserve-metadata=entitlements"
        )
        try await codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/Autoupdate",
            options: "-f -o runtime"
        )
        try await codesign(
            identity: codeSignIdentity,
            path: "\(sparkleFrameworkPath)/Versions/B/Updater.app",
            options: "-f -o runtime"
        )
        try await codesign(
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
        try await saveCodesignEntitlements(target: appExtensionPath, path: tmpEntitlementXmlPath)
        // Strip the get-task-allow entitlement from the XML entitlements file
        let xmlEntitlements = try String(contentsOfFile: tmpEntitlementXmlPath)
        let entitlementKeyValuePair = "<key>com.apple.security.get-task-allow</key><true/>"
        let strippedEntitlements =
            xmlEntitlements.replacingOccurrences(of: entitlementKeyValuePair, with: "")
        try strippedEntitlements.write(toFile: tmpEntitlementXmlPath,
                                       atomically: true,
                                       encoding: .utf8)
        try await codesign(identity: codeSignIdentity,
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
    try await recursivelyCodesign(path: binariesDir, identity: codeSignIdentity, skip: [mainExecutablePath])

    var mainExecutableCodesignOptions = defaultCodesignOptions
    if let entitlementsPath {
        mainExecutableCodesignOptions =
            "--timestamp --force --verbose=4 --options runtime --entitlements \"\(entitlementsPath)\""
    }
    try await codesign(
        identity: codeSignIdentity, path: mainExecutablePath, options: mainExecutableCodesignOptions
    )
}
