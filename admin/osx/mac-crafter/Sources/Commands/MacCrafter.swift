// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

///
/// Main command of this application.
///
@main
struct MacCrafter: AsyncParsableCommand {
    static let configuration = CommandConfiguration(
        abstract: "A tool to easily build a fully-functional Nextcloud Desktop Client for macOS.",
        subcommands: [Build.self, Codesign.self, Package.self, CreateDMG.self],
        defaultSubcommand: Build.self
    )
}
