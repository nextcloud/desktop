//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

///
/// Errors specific to the domain of this app.
///
enum FileProviderAppError: Error {
    ///
    /// The file provider domain required for the action does not exist.
    ///
    case fileProviderDomainNotFound
}
