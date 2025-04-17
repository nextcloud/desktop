/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import Foundation

weak var globalTaskRef: Process?

@discardableResult
func run(
    _ launchPath: String,
    _ args: [String],
    env: [String: String]? = nil,
    quiet: Bool = false,
    task: Process = Process()
) -> Int32 {
    globalTaskRef = task
    signal(SIGINT) { _ in
        globalTaskRef?.terminate()  // Send terminate signal to the task
        exit(0)           // Exit the script after cleanup
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

func run(
    _ launchPath: String,
    _ args: String...,
    env: [String: String]? = nil,
    quiet: Bool = false
) -> Int32 {
    return run(launchPath, args, env: env, quiet: quiet)
}

@discardableResult
func shell(_ commands: String..., env: [String: String]? = nil, quiet: Bool = false) -> Int32 {
    return run("/bin/zsh", ["-c"] + commands, env: env, quiet: quiet)
}

func commandExists(_ command: String) -> Bool {
    return run("/usr/bin/type", command, quiet: true) == 0
}
