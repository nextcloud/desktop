//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

///
/// How far to traverse down the hierarchy.
///
public enum EnumerateDepth: String {
    ///
    /// Only the item itself.
    ///
    case target = "0"

    ///
    /// The item itself and its direct descendants.
    ///
    case targetAndDirectChildren = "1"

    ///
    /// All the way down, even to the farthest descendant.
    ///
    case targetAndAllChildren = "infinity"
}
