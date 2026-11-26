// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

struct CraftMasterConfiguration: Decodable {
    static let kdeBlueprintsRevisionKey = "craft/craft-blueprints-kde.revision"
    static let clientBlueprintsRevisionKey = "craft/craft-blueprints-nextcloud.revision"

    let kdeBlueprintsRevision: String?
    let clientBlueprintsRevision: String?
    let compileBuildType: String?
    let blueprintBuildType: String?
    let buildTests: Bool?

    init(
        at file: URL,
        target: String,
        blueprint: String,
        craftMasterDirectory: URL,
        overrideFile: URL? = nil,
        buildTestsOverride: Bool? = nil
    ) throws {
        guard let reader = Bundle.module.url(forResource: "read_craftmaster_config", withExtension: "py") else {
            throw MacCrafterError.craftError("CraftMaster configuration reader is missing from mac-crafter's resources.")
        }

        let task = Process()
        let output = Pipe()
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
        ]
        task.standardOutput = output
        task.standardError = FileHandle.standardError
        try task.run()

        let data = output.fileHandleForReading.readDataToEndOfFile()
        task.waitUntilExit()
        guard task.terminationStatus == 0 else {
            let message = String(decoding: data, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines)
            let details = message.isEmpty ? "configuration reader exited with status \(task.terminationStatus)" : message
            throw MacCrafterError.craftError("Unable to read \(file.path): \(details)")
        }

        self = try JSONDecoder().decode(Self.self, from: data)
    }
}
