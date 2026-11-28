// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import os

struct CraftMasterConfiguration: Decodable {
    static let kdeBlueprintsRevisionKey = "craft/craft-blueprints-kde.revision"
    static let clientBlueprintsRevisionKey = "craft/craft-blueprints-nextcloud.revision"

    let kdeBlueprintsRevision: String?
    let clientBlueprintsRevision: String?
    let compileBuildType: String?
    let blueprintBuildType: String?
    let buildTests: Bool?
    let workOffline: Bool?
    let blueprintRoot: String?

    init(
        at file: URL,
        target: String,
        blueprint: String,
        craftMasterDirectory: URL,
        overrideFile: URL? = nil,
        buildTestsOverride: Bool? = nil,
        blueprintRootOnly: Bool = false
    ) throws {
        guard let reader = Bundle.module.url(forResource: "read_craftmaster_config", withExtension: "py") else {
            throw MacCrafterError.craftError("CraftMaster configuration reader is missing from mac-crafter's resources.")
        }

        let task = Process()
        let output = Pipe()
        let errors = Pipe()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/env")
        task.arguments = [
            "python3",
            "-I",
            "-B",
            reader.path,
            file.path,
            target,
            blueprint,
            craftMasterDirectory.path,
            Self.kdeBlueprintsRevisionKey,
            Self.clientBlueprintsRevisionKey,
            overrideFile?.path ?? "",
            buildTestsOverride.map { String($0) } ?? "",
            String(blueprintRootOnly),
        ]
        task.standardOutput = output
        task.standardError = errors
        try task.run()

        let diagnostics = OSAllocatedUnfairLock(initialState: Data())
        let readDiagnostics = DispatchGroup()
        readDiagnostics.enter()
        // Drain stderr independently of the executor while the caller blocks on stdout.
        Thread.detachNewThread {
            let data = errors.fileHandleForReading.readDataToEndOfFile()
            diagnostics.withLock { $0 = data }
            readDiagnostics.leave()
        }
        let data = output.fileHandleForReading.readDataToEndOfFile()
        task.waitUntilExit()
        readDiagnostics.wait()

        let stderr = diagnostics.withLock { String(decoding: $0, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines) }
        if !stderr.isEmpty {
            Log.error(stderr)
        }
        guard task.terminationStatus == 0 else {
            let stdout = String(decoding: data, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines)
            let message = [stderr, stdout].filter { !$0.isEmpty }.joined(separator: "\n")
            let details = message.isEmpty ? "configuration reader exited with status \(task.terminationStatus)" : message
            throw MacCrafterError.craftError("Unable to read \(file.path): \(details)")
        }

        self = try JSONDecoder().decode(Self.self, from: data)
    }
}
