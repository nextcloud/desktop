/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation

func archToCraftTarget(_ arch: String) -> String {
    return arch == "arm64" ? "macos-clang-arm64" : "macos-64-clang"
}

func craftBlueprintRepository(_ url: String, branch: String?, revision: String?) async throws -> String {
    if let branch {
        return "\(url)|\(branch)|"
    }
    guard let revision else {
        return "\(url)|master|"
    }

    let branchPrefix = "refs/heads/"
    let name = revision.hasPrefix(branchPrefix) ? String(revision.dropFirst(branchPrefix.count)) : revision
    let ref = "\(branchPrefix)\(name)"
    let task = Process()
    let output = Pipe()
    task.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    task.arguments = [
        "git",
        "ls-remote",
        "--exit-code",
        "--heads",
        "--",
        url,
        ref,
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

    let branches = String(decoding: data, as: UTF8.self).split(separator: "\n")
    if branches.contains(where: { $0.hasSuffix("\t\(ref)") }) {
        return "\(url)|\(name)|"
    }
    return "\(url)||\(revision)"
}
