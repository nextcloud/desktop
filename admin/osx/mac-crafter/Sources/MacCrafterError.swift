// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

enum MacCrafterError: Error, CustomStringConvertible {
    case craftError(String)
    case downloadError(String)
    case environmentError(String)
    case failedEnumeration(String)
    case gitError(String)
    case signing(String)

    var description: String {
        switch self {
            case .craftError(let message):
                return "Craft: \(message)"
            case .downloadError(let message):
                return "Download: \(message)"
            case .environmentError(let message):
                return "Environment: \(message)"
            case .failedEnumeration(let message):
                return "Failed enumeration: \(message)"
            case .gitError(let message):
                return "Git: \(message)"
            case .signing(let message):
                return "Signing: \(message)"
        }
    }
}
