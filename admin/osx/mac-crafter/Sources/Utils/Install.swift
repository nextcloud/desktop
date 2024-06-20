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
