// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Run a shell command.
///
/// - Parameters:
///     - launchPath: The command to run.
///     - args: Arguments to pass.
///     - env: Environment variables to pass for this specific command.
///     - quiet: Whether the standard and error output of the command should be printed or omitted.
///
/// - Returns: Exit code of the command.
///
@discardableResult
func run(
    _ launchPath: String,
    _ args: [String],
    env: [String: String]? = nil,
    quiet: Bool = false,
    task: Process = Process()
) async -> Int32 {
    await State.shared.register(task)

    signal(SIGINT) { _ in
        Task {
            await State.shared.terminate()
            exit(0)
        }
    }

    task.launchPath = launchPath
    task.arguments = args

    if let env,
       let combinedEnv = task.environment?.merging(env, uniquingKeysWith: { (_, new) in new })
    {
        task.environment = combinedEnv
    }

    if quiet {
        task.standardOutput = nil
        task.standardError = nil
    }

    task.launch()
    task.waitUntilExit()

    return task.terminationStatus
}

///
/// Run a shell command.
///
/// - Parameters:
///     - launchPath: The command to run.
///     - args: Arguments to pass.
///     - env: Environment variables to pass for this specific command.
///     - quiet: Whether the standard and error output of the command should be printed or omitted.
///
/// - Returns: Exit code of the command.
///
func run(
    _ launchPath: String,
    _ args: String...,
    env: [String: String]? = nil,
    quiet: Bool = false
) async -> Int32 {
    return await run(launchPath, args, env: env, quiet: quiet)
}

///
/// Run multiple shell commands.
///
/// - Returns: Exit code of the command.
///
@discardableResult
func shell(_ commands: String..., env: [String: String]? = nil, quiet: Bool = false) async -> Int32 {
    return await run("/bin/zsh", ["-c"] + commands, env: env, quiet: quiet)
}

///
/// Check whether the given shell command is available in the shell.
///
/// - Parameters:
///     - command: The command to check for availability.
///
/// - Returns: `true` in case of availability, otherwise `false`.
///
func commandExists(_ command: String) async -> Bool {
    return await run("/usr/bin/type", command, quiet: true) == 0
}
