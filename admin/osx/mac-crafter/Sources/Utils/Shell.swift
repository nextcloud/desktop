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

    var stdoutPipe: Pipe?
    var stderrPipe: Pipe?

    if let env,
       let combinedEnv = task.environment?.merging(env, uniquingKeysWith: { (_, new) in new })
    {
        task.environment = combinedEnv
    }

    if quiet {
        task.standardOutput = nil
        task.standardError = nil
    } else {
        stdoutPipe = Pipe()
        stderrPipe = Pipe()
        task.standardOutput = stdoutPipe
        task.standardError = stderrPipe

        // Stream child output into our logging facility.
        let forward: (Pipe?, @Sendable @escaping (String) -> Void) -> Void = { pipe, logger in
            pipe?.fileHandleForReading.readabilityHandler = { handle in
                let data = handle.availableData

                guard !data.isEmpty else {
                    handle.readabilityHandler = nil
                    return
                }

                guard let text = String(data: data, encoding: .utf8), !text.isEmpty else {
                    return
                }

                text.split(separator: "\n", omittingEmptySubsequences: true).forEach { line in
                    logger(String(line))
                }
            }
        }

        forward(stdoutPipe) {
            Log.info($0)
        }

        forward(stderrPipe) {
            Log.error($0)
        }
    }

    task.launch()
    task.waitUntilExit()

    // Ensure we stop observing and collect any remaining buffered output.
    func flush(_ pipe: Pipe?, to logger: (String) -> Void) {
        pipe?.fileHandleForReading.readabilityHandler = nil

        if let data = try? pipe?.fileHandleForReading.readToEnd(),
           let text = String(data: data, encoding: .utf8),
           !text.isEmpty {
            text.split(separator: "\n", omittingEmptySubsequences: true).forEach { line in
                logger(String(line))
            }
        }
    }

    if quiet == false {
        flush(stdoutPipe) {
            Log.info($0)
        }

        flush(stderrPipe) {
            Log.error($0)
        }
    }

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
