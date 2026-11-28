// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing
@testable import mac_crafter

struct CraftBlueprintRepositoryTests {
    private let missingRepository = FileManager.default.temporaryDirectory
        .appendingPathComponent("missing mac-crafter repository \(UUID())").path

    @Test
    func distinguishesMovingBranchesFromPinnedRevisions() async throws {
        let repository = FileManager.default.temporaryDirectory
            .appendingPathComponent("mac-crafter repository \(UUID())")
        try FileManager.default.createDirectory(at: repository, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: repository) }

        let branch = "stable/release"
        _ = try git(["init", "--initial-branch", branch], in: repository)
        let commitArguments = [
            "-c",
            "user.name=mac-crafter tests",
            "-c",
            "user.email=mac-crafter@example.invalid",
            "-c",
            "commit.gpgsign=false",
            "-c",
            "core.hooksPath=/dev/null",
            "commit",
            "--allow-empty",
            "-m",
        ]
        _ = try git(commitArguments + ["Blueprint fixture"], in: repository)
        _ = try git(["-c", "tag.gpgSign=false", "tag", "pinned"], in: repository)
        _ = try git(["-c", "tag.gpgSign=false", "tag", "ambiguous"], in: repository)
        _ = try git(["branch", "topic/refs/heads/pinned"], in: repository)
        let commit = try git(["rev-parse", "HEAD"], in: repository)
        _ = try git(commitArguments + ["Moving branch fixture"], in: repository)
        _ = try git(["branch", "ambiguous"], in: repository)
        let path = repository.path
        let craftURL = "[git]\(repository.absoluteString)"

        #expect(try await craftBlueprintRepository(path, branch: nil, revision: branch) == "\(path)|\(branch)|")
        #expect(try await craftBlueprintRepository(craftURL, branch: nil, revision: branch) == "\(craftURL)|\(branch)|")
        #expect(try await craftBlueprintRepository(path, branch: nil, revision: "refs/heads/\(branch)") == "\(path)|\(branch)|")
        #expect(try await craftBlueprintRepository(path, branch: nil, revision: "refs/heads/ambiguous") == "\(path)|ambiguous|")
        #expect(try await craftBlueprintRepository(path, branch: nil, revision: "pinned") == "\(path)||pinned")
        #expect(try await craftBlueprintRepository(path, branch: nil, revision: "ambiguous") == "\(path)||refs/tags/ambiguous")
        #expect(try await craftBlueprintRepository(path, branch: nil, revision: "refs/tags/pinned") == "\(path)||refs/tags/pinned")
        #expect(try await craftBlueprintRepository(path, branch: nil, revision: commit) == "\(path)||\(commit)")
    }

    @Test
    func usesExplicitAndFallbackBranchesWithoutContactingRemote() async throws {
        #expect(try await craftBlueprintRepository(missingRepository, branch: "custom", revision: "stable") == "\(missingRepository)|custom|")
        #expect(try await craftBlueprintRepository(missingRepository, branch: nil, revision: nil) == "\(missingRepository)|master|")
    }

    @Test(arguments: ["refs/tags/pinned", String(repeating: "a", count: 40), String(repeating: "a", count: 64)])
    func preservesExplicitPinsWithoutContactingRemote(_ revision: String) async throws {
        #expect(try await craftBlueprintRepository(missingRepository, branch: nil, revision: revision) == "\(missingRepository)||\(revision)")
    }

    @Test
    func preservesConfiguredRevisionWithoutContactingRemoteInOfflineMode() async throws {
        #expect(try await craftBlueprintRepository(missingRepository, branch: nil, revision: "stable", offline: true) == "\(missingRepository)||stable")
    }

    @Test
    func failsWhenRemoteCannotBeQueried() async {
        await #expect(throws: MacCrafterError.self) {
            try await craftBlueprintRepository(missingRepository, branch: nil, revision: "stable")
        }
    }

    private func git(_ arguments: [String], in repository: URL) throws -> String {
        let process = Process()
        let output = Pipe()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
        process.arguments = ["git", "-C", repository.path] + arguments
        process.standardOutput = output
        try process.run()

        let data = output.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        try #require(process.terminationStatus == 0)
        return String(decoding: data, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
