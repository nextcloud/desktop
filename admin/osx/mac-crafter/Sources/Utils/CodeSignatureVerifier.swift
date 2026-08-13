// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Discovers signed code components and verifies their team identifiers.
///
enum CodeSignatureVerifier {
    static func verify(at location: URL) throws {
        try verify(
            at: location,
            isMachO: { isMachO($0) },
            signatureDetails: { try codesignDetails(at: $0) }
        )
    }

    static func verify(
        at location: URL,
        isMachO: (URL) -> Bool,
        signatureDetails: (URL) throws -> String
    ) throws {
        let components = try discoverCodeComponents(at: location, isMachO: isMachO)
        let signatures = try components.map { component in
            (
                location: component.path,
                teamIdentifier: TeamIdentifierVerifier.teamIdentifier(from: try signatureDetails(component))
            )
        }

        if let error = TeamIdentifierVerifier.validationError(for: signatures) {
            throw MacCrafterError.signing(error)
        }

        Log.info("Verified matching TeamIdentifier for \(signatures.count) code components")
    }

    static func discoverCodeComponents(at url: URL, isMachO: (URL) -> Bool) throws -> [URL] {
        let codeBundleExtensions = ["app", "appex", "framework", "xpc"]
        let codeSearchDirectories = ["/Contents/MacOS/", "/Contents/Frameworks/", "/Contents/PlugIns/"]
        var components = [URL]()

        guard let enumerator = FileManager.default.enumerator(
            at: url,
            includingPropertiesForKeys: [.isRegularFileKey]
        ) else {
            throw MacCrafterError.environmentError("Failed to get enumerator for: \(url.path)")
        }

        for case let candidate as URL in enumerator {
            let pathExtension = candidate.pathExtension.lowercased()

            if codeBundleExtensions.contains(pathExtension) || pathExtension == "dylib" {
                components.append(candidate)
                continue
            }

            guard codeSearchDirectories.contains(where: candidate.path.contains),
                  try candidate.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile == true,
                  isMachO(candidate)
            else {
                continue
            }

            components.append(candidate)
        }

        return [url] + components.sorted { $0.path < $1.path }
    }

    private static func isMachO(_ file: URL) -> Bool {
        let task = Process()
        let outputPipe = Pipe()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/file")
        task.arguments = ["-b", file.path]
        task.standardOutput = outputPipe
        task.standardError = Pipe()

        do {
            try task.run()
        } catch {
            return false
        }

        let output = String(data: outputPipe.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        task.waitUntilExit()
        return task.terminationStatus == 0 && output.contains("Mach-O")
    }

    private static func codesignDetails(at location: URL) throws -> String {
        let task = Process()
        let standardOutput = Pipe()
        let standardError = Pipe()
        task.executableURL = URL(fileURLWithPath: "/usr/bin/codesign")
        task.arguments = ["--display", "--verbose=4", location.path]
        task.standardOutput = standardOutput
        task.standardError = standardError

        do {
            try task.run()
        } catch {
            throw MacCrafterError.signing("Unable to inspect the code signature of \(location.path): \(error.localizedDescription)")
        }

        let outputData = standardOutput.fileHandleForReading.readDataToEndOfFile()
        let errorData = standardError.fileHandleForReading.readDataToEndOfFile()
        task.waitUntilExit()

        guard task.terminationStatus == 0 else {
            let errorOutput = String(data: errorData, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines) ?? "unknown codesign error"
            throw MacCrafterError.signing("Unable to inspect the code signature of \(location.path): \(errorOutput)")
        }

        return [outputData, errorData]
            .compactMap { String(data: $0, encoding: .utf8) }
            .joined(separator: "\n")
    }
}
