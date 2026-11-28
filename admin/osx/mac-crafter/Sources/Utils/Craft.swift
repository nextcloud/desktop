/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation

func archToCraftTarget(_ arch: String) -> String {
    return arch == "arm64" ? "macos-clang-arm64" : "macos-64-clang"
}

func craftGitURL(_ url: String) -> String {
    url.replacingOccurrences(of: "[git]", with: "")
}

func craftBlueprintRepository(_ url: String, branch: String?, revision: String?, offline: Bool = false) async throws -> String {
    if let branch {
        return "\(url)|\(branch)|"
    }
    guard let revision else {
        return "\(url)|master|"
    }

    let branchPrefix = "refs/heads/"
    if revision.hasPrefix(branchPrefix) {
        return "\(url)|\(revision.dropFirst(branchPrefix.count))|"
    }
    let tagPrefix = "refs/tags/"
    let isObjectID = revision.wholeMatch(of: #/[0-9a-fA-F]{40}|[0-9a-fA-F]{64}/#) != nil
    let pinnedRepository = "\(url)||\(revision)"
    if offline || revision.hasPrefix(tagPrefix) || isObjectID {
        return pinnedRepository
    }

    let branchRef = "\(branchPrefix)\(revision)"
    let tagRef = "\(tagPrefix)\(revision)"
    let task = Process()
    let output = Pipe()
    task.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    task.arguments = [
        "git",
        "ls-remote",
        "--exit-code",
        "--refs",
        "--",
        craftGitURL(url),
        branchRef,
        tagRef,
    ]
    task.standardOutput = output
    task.standardError = FileHandle.standardError
    await State.shared.register(task)
    try task.run()

    let data = output.fileHandleForReading.readDataToEndOfFile()
    task.waitUntilExit()
    let status = task.terminationStatus
    guard status == 0 || status == 2 else {
        throw MacCrafterError.gitError("Unable to resolve blueprint revision \(revision). Git exited with status \(status).")
    }

    let refs = String(decoding: data, as: UTF8.self).split(separator: "\n")
    guard refs.contains(where: { $0.hasSuffix("\t\(branchRef)") }) else { return pinnedRepository }
    let hasTag = refs.contains { $0.hasSuffix("\t\(tagRef)") }
    return hasTag ? "\(url)||\(tagRef)" : "\(url)|\(revision)|"
}
