// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Global state object.
///
actor State {
    weak private var process: Process?

    static let shared = State()

    ///
    /// Register the shell command process.
    ///
    func register(_ process: Process) {
        self.process = process
    }

    ///
    /// Terminate any previously registered shell command process.
    ///
    /// Silently fails in case no process is registered.
    ///
    func terminate() {
        process?.terminate()
    }
}
