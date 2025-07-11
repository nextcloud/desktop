// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Errors which can occur during installation of a command-line program.
///
enum InstallError: Error {
    ///
    /// The installation failed in general with the given output provided by the installation command.
    ///
    case failedToInstall(String)
}

///
/// Install a command-line program if not available already.
///
/// - Parameters:
///     - command: The command name which is required.
///     - installCommand: The installation command to install the command, if it is not yet installed.
///     - installCommandEnv: Optional environment variables for the installation command.
///
func installIfMissing(
    _ command: String,
    _ installCommand: String,
    installCommandEnv: [String: String]? = nil
) async throws {
    if await commandExists(command) {
        print("\(command) is installed.")
    } else {
        print("\(command) is missing. Installing...")
        guard await shell(installCommand, env: installCommandEnv) == 0 else {
            throw InstallError.failedToInstall("Failed to install \(command).")
        }
        print("\(command) installed.")
    }
}
