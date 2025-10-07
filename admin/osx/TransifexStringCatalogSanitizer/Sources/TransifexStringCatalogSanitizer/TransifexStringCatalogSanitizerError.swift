//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

enum TransifexStringCatalogSanitizerError: Error {
    case jsonObject
    case missingLocalization
    case missingLocalizations
    case missingString
    case missingStrings
    case missingStringUnit
    case missingValue
}
