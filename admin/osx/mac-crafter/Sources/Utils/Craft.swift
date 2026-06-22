/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

func archToCraftTarget(_ arch: String) -> String {
    return arch == "arm64" ? "macos-clang-arm64" : "macos-64-clang"
}
