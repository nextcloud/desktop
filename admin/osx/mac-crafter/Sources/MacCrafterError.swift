// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

enum MacCrafterError: Error, CustomStringConvertible {
    case failedEnumeration(String)
    case environmentError(String)
    case gitError(String)
    case craftError(String)

    var description: String {
        switch self {
            case .failedEnumeration(let message):
                return "Failed enumeration: \(message)"
            case .environmentError(let message):
                return "Environment: \(message)"
            case .gitError(let message):
                return "Git: \(message)"
            case .craftError(let message):
                return "Craft: \(message)"
        }
    }
}
