// SPDX-FileCopyrightText: 2024 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

enum InstallError: Error {
    case failedToInstall(String)
}

func installIfMissing(
    _ command: String,
    _ installCommand: String,
    installCommandEnv: [String: String]? = nil
) throws {
    if commandExists(command) {
        print("\(command) is installed.")
    } else {
        print("\(command) is missing. Installing...")
        guard shell(installCommand, env: installCommandEnv) == 0 else {
            throw InstallError.failedToInstall("Failed to install \(command).")
        }
        print("\(command) installed.")
    }
}
