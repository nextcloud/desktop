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

enum CodeSigningError: Error {
    case failedToCodeSign(String)
}

enum AppBundleSigningError: Error {
    case couldNotEnumerate(String)
}

func isLibrary(_ path: String) -> Bool {
    path.hasSuffix(".dylib") || path.hasSuffix(".framework")
}

func isAppExtension(_ path: String) -> Bool {
    path.hasSuffix(".appex")
}

func codesign(
    identity: String,
    path: String,
    options: String = "--timestamp --force --preserve-metadata=entitlements --verbose=4 --options runtime"
) throws {
    print("Code-signing \(path)...")
    let command = "codesign -s \"\(identity)\" \(options) \(path)"
    guard shell(command) == 0 else {
        throw CodeSigningError.failedToCodeSign("Failed to code-sign \(path).")
    }
}

func recursivelyCodesign(path: String, identity: String) throws {
    let fm = FileManager.default
    guard let pathEnumerator = fm.enumerator(atPath: path) else {
        throw AppBundleSigningError.couldNotEnumerate(
            "Failed to enumerate directory at \(path)."
        )
    }

    for case let enumeratedItem as String in pathEnumerator {
        guard isLibrary(enumeratedItem) || isAppExtension(enumeratedItem) else { continue }
        try codesign(identity: identity, path: "\(path)/\(enumeratedItem)")
    }
}

func codesignClientAppBundle(
    at clientAppDir: String, withCodeSignIdentity codeSignIdentity: String
) throws {
    print("Code-signing Nextcloud Desktop Client libraries, frameworks and plugins...")

    let clientContentsDir = "\(clientAppDir)/Contents"

    try recursivelyCodesign(path: "\(clientContentsDir)/Frameworks", identity: codeSignIdentity)
    try recursivelyCodesign(path: "\(clientContentsDir)/PlugIns", identity: codeSignIdentity)
    try recursivelyCodesign(path: "\(clientContentsDir)/Resources", identity: codeSignIdentity)

    print("Code-signing Nextcloud Desktop Client app bundle...")
    try codesign(identity: codeSignIdentity, path: clientAppDir)
}
