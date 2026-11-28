// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing
@testable import mac_crafter

struct CraftMasterConfigurationTests {
    private let target = "macos-clang-arm64"

    private func withConfiguration(
        _ contents: String,
        fixtureSuffix: String = "",
        validate: (URL, URL) throws -> Void
    ) throws {
        let fileManager = FileManager.default
        let directory = fileManager.temporaryDirectory
            .appendingPathComponent("mac-crafter test \(UUID().uuidString)")
        defer { try? fileManager.removeItem(at: directory) }

        let craftMasterDirectory = directory.appendingPathComponent("craftmaster")
        try fileManager.createDirectory(at: craftMasterDirectory, withIntermediateDirectories: true)
        let fixture = try #require(Bundle.module.url(forResource: "Config", withExtension: "py"))
        let parser = try String(contentsOf: fixture, encoding: .utf8) + fixtureSuffix
        try parser.write(to: craftMasterDirectory.appendingPathComponent("Config.py"), atomically: true, encoding: .utf8)

        let file = directory.appendingPathComponent("craftmaster.ini")
        try contents.write(to: file, atomically: true, encoding: .utf8)
        try validate(file, craftMasterDirectory)
    }

    private func readConfiguration(at file: URL, target: String, craftMasterDirectory: URL) throws -> CraftMasterConfiguration {
        try CraftMasterConfiguration(
            at: file,
            target: target,
            blueprint: "nextcloud-client",
            craftMasterDirectory: craftMasterDirectory
        )
    }

    @Test
    func readsCraftMasterSettingsWithoutWritingConfigDump() throws {
        let contents = """
        [DEFAULT]
        craft/craft-blueprints-kde.revision = ${Variables:KdeRevision}

        [General]
        DumpConfig = True

        [Variables]
        KdeRevision = stable-kde
        BuildType = Debug

        [GeneralSettings]
        Compile/BuildType : Release

        [BlueprintSettings]
        nextcloud-client.buildTests = Off
        nextcloud-client.buildtests = On

        [macos-clang-arm64-GeneralSettings]
        Compile/BuildType = MinSizeRel

        [macos-clang-arm64]
        General/Description = A setting with a continuation
            Compile/BuildType = RelWithDebInfo

        [macos-64-clang-GeneralSettings]
        Compile/BuildType = MinSizeRel

        [macos-64-clang]
        Compile/BuildType = ${Variables:BuildType}
        """

        try withConfiguration(contents) { file, craftMasterDirectory in
            let arm = try readConfiguration(at: file, target: target, craftMasterDirectory: craftMasterDirectory)
            let intel = try readConfiguration(at: file, target: "macos-64-clang", craftMasterDirectory: craftMasterDirectory)

            #expect(arm.kdeBlueprintsRevision == "stable-kde")
            #expect(arm.clientBlueprintsRevision == nil)
            #expect(arm.compileBuildType == "MinSizeRel")
            #expect(arm.blueprintBuildType == nil)
            #expect(arm.buildTests == false)
            #expect(intel.compileBuildType == "Debug")
            #expect(!FileManager.default.fileExists(atPath: file.appendingPathExtension("dump").path))
        }
    }

    @Test
    func readsSettingsWhenCraftMasterWritesDiagnostics() throws {
        let diagnostic = "\nimport sys\nprint('CraftMaster fixture diagnostic', file=sys.stderr)\n"

        try withConfiguration("[GeneralSettings]\nCompile/BuildType = Debug\n", fixtureSuffix: diagnostic) { file, craftMasterDirectory in
            let settings = try readConfiguration(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.compileBuildType == "Debug")
        }
    }

    @Test
    func buildUsesConfiguredDefaults() throws {
        let contents = """
        [GeneralSettings]
        Compile/BuildType = Release

        [BlueprintSettings]
        craft/craft-blueprints-kde.revision = stable-kde
        craft/craft-blueprints-nextcloud.revision = stable-client
        nextcloud-client.buildTests = False

        [macos-clang-arm64-BlueprintSettings]
        craft/craft-blueprints-nextcloud.revision = 6e684481b778f177944642f588e6e6f3ed95f558
        nextcloud-client.buildType = MinSizeRel
        nextcloud-client.buildTests = Yes
        """

        try withConfiguration(contents) { file, craftMasterDirectory in
            let build = try Build.parse([])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.kdeBlueprintsRevision == "stable-kde")
            #expect(settings.clientBlueprintsRevision == "6e684481b778f177944642f588e6e6f3ed95f558")
            #expect(settings.compileBuildType == "Release")
            #expect(settings.buildType == "MinSizeRel")
            #expect(settings.buildTests)
        }
    }

    @Test
    func customRepositoryURLsDoNotInheritStockRevisions() throws {
        let contents = """
        [BlueprintSettings]
        craft/craft-blueprints-kde.revision = stable-kde
        craft/craft-blueprints-nextcloud.revision = stable-client
        """

        try withConfiguration(contents) { file, craftMasterDirectory in
            let build = try Build.parse([
                "--kde-blueprints-git-url=https://example.test/kde-fork.git",
                "--client-blueprints-git-url=https://example.test/client-fork.git",
            ])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.kdeBlueprintsRevision == nil)
            #expect(settings.clientBlueprintsRevision == nil)
        }
    }

    @Test
    func buildUsesConfiguredOfflineMode() throws {
        let contents = """
        [GeneralSettings]
        General/WorkOffline = Off

        [\(target)-GeneralSettings]
        General/WorkOffline = No

        [\(target)]
        General/WorkOffline = Yes
        """

        try withConfiguration(contents) { file, craftMasterDirectory in
            let build = try Build.parse([])
            let arm = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)
            let intel = try build.craftSettings(at: file, target: "macos-64-clang", craftMasterDirectory: craftMasterDirectory)

            #expect(arm.workOffline)
            #expect(!intel.workOffline)
        }
    }

    @Test(arguments: ["--build-tests", "--no-build-tests"])
    func commandLineOverridesConfiguredValues(_ testFlag: String) throws {
        let configuredBuildTests = testFlag == "--build-tests" ? "False" : "True"
        let contents = """
        [Variables]
        Revision = stable
        BuildType = Release

        [GeneralSettings]
        Compile/BuildType = ${Variables:BuildType}

        [BlueprintSettings]
        craft/craft-blueprints-kde.revision = ${Variables:Revision}
        craft/craft-blueprints-nextcloud.revision = ${Variables:Revision}
        nextcloud-client.buildType = MinSizeRel
        nextcloud-client.buildTests = \(configuredBuildTests)
        """

        try withConfiguration(contents) { file, craftMasterDirectory in
            let build = try Build.parse([
                "--build-type=Debug",
                testFlag,
            ])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.kdeBlueprintsRevision == "stable")
            #expect(settings.clientBlueprintsRevision == "stable")
            #expect(settings.compileBuildType == "Debug")
            #expect(settings.buildType == "Debug")
            #expect(settings.buildTests == (testFlag == "--build-tests"))
        }
    }

    @Test(arguments: ["--build-tests", "--no-build-tests"])
    func commandLineOverridesInvalidBuildTestsValue(_ testFlag: String) throws {
        try withConfiguration("[BlueprintSettings]\nnextcloud-client.buildTests = maybe\n") { file, craftMasterDirectory in
            let build = try Build.parse([testFlag])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.buildTests == (testFlag == "--build-tests"))
        }
    }

    @Test
    func matchingBlueprintOverrideReplacesConfiguredBranchAndRevision() throws {
        let contents = """
        [BlueprintSettings]
        craft/craft-blueprints-kde.branch = configured-kde
        craft/craft-blueprints-kde.revision = ${Variables:MissingRevision}
        craft/craft-blueprints-nextcloud.revision = stable-client

        [\(target)-BlueprintSettings]
        craft/craft-blueprints-kde.branch = target-kde
        craft/craft-blueprints-kde.revision = ${Variables:MissingRevision}
        """

        try withConfiguration(contents) { file, craftMasterDirectory in
            let build = try Build.parse([
                "--kde-blueprints-git-ref=custom-kde",
                "--client-blueprints-git-ref=custom-client",
            ])
            let directory = file.deletingLastPathComponent()
            let craftTargetURL = directory.appendingPathComponent(target)
            let blueprintRoot = try build.craftBlueprintRoot(
                at: file,
                target: target,
                craftMasterDirectory: craftMasterDirectory,
                craftTargetURL: craftTargetURL
            )
            let generatedOverride = try build.blueprintOverrides(target: target, blueprintRoot: blueprintRoot)
            let overlay = try #require(generatedOverride)
            let lines = overlay.split(whereSeparator: \.isNewline).map { $0.trimmingCharacters(in: .whitespaces) }

            #expect(lines.contains("[BlueprintSettings]"))
            #expect(lines.contains("[\(target)-BlueprintSettings]"))
            #expect(lines.filter { $0 == "craft/craft-blueprints-kde.branch = custom-kde" }.count == 2)
            #expect(lines.filter { $0 == "craft/craft-blueprints-kde.revision =" }.count == 2)
            #expect(!overlay.contains("craft/craft-blueprints-nextcloud."))

            let overrideFile = directory.appendingPathComponent("override.ini")
            try overlay.write(to: overrideFile, atomically: true, encoding: .utf8)
            let settings = try build.craftSettings(
                at: file,
                target: target,
                craftMasterDirectory: craftMasterDirectory,
                overrideFile: overrideFile
            )

            #expect(settings.kdeBlueprintsRevision == nil)
            #expect(settings.clientBlueprintsRevision == "stable-client")
        }
    }

    @Test
    func unrelatedBlueprintOverrideDoesNotChangeCraftConfiguration() throws {
        try withConfiguration("") { file, _ in
            let build = try Build.parse([
                "--kde-blueprints-git-url=https://example.test/different.git",
                "--kde-blueprints-git-ref=custom-kde",
                "--client-blueprints-git-ref=custom-client",
            ])
            let blueprintRoot = file.deletingLastPathComponent().appendingPathComponent("blueprints")

            #expect(try build.blueprintOverrides(target: target, blueprintRoot: blueprintRoot) == nil)
        }
    }

    @Test
    func kdeOverrideFollowsLegacyCheckoutWhenPresent() throws {
        try withConfiguration("") { file, _ in
            let blueprintRoot = file.deletingLastPathComponent().appendingPathComponent("blueprints")
            let legacyCheckout = blueprintRoot.appendingPathComponent("craft-kde")
            let arguments = ["--kde-blueprints-git-ref=stable"]
            let current = try Build.parse(arguments)
            let legacy = try Build.parse(arguments + ["--kde-blueprints-git-url=git@example.test:craft-kde.git"])

            #expect(try legacy.blueprintOverrides(target: target, blueprintRoot: blueprintRoot) == nil)
            #expect(try legacy.blueprintOverrides(target: target, blueprintRoot: blueprintRoot, registeringBlueprints: true) != nil)

            try FileManager.default.createDirectory(at: legacyCheckout, withIntermediateDirectories: true)

            #expect(try legacy.blueprintOverrides(target: target, blueprintRoot: blueprintRoot) != nil)
            #expect(try current.blueprintOverrides(target: target, blueprintRoot: blueprintRoot) == nil)
        }
    }

    @Test
    func targetBlueprintRootControlsWhichCheckoutReceivesTheOverride() throws {
        try withConfiguration("") { file, craftMasterDirectory in
            let directory = file.deletingLastPathComponent()
            let craftTargetURL = directory.appendingPathComponent(target)
            let targetRoot = directory.appendingPathComponent("configured blueprints")
            let contents = """
            [Variables]
            SelectedRoot = \(targetRoot.path)

            [GeneralSettings]
            Blueprints/BlueprintRoot = \(directory.appendingPathComponent("global").path)

            [\(target)-GeneralSettings]
            Blueprints/BlueprintRoot = \(directory.appendingPathComponent("legacy target").path)

            [\(target)]
            Blueprints/BlueprintRoot = ${Variables:SelectedRoot}
            """
            try contents.write(to: file, atomically: true, encoding: .utf8)
            try FileManager.default.createDirectory(
                at: targetRoot.appendingPathComponent("craft-kde"),
                withIntermediateDirectories: true
            )

            let arguments = ["--kde-blueprints-git-ref=stable"]
            let current = try Build.parse(arguments)
            let legacy = try Build.parse(arguments + ["--kde-blueprints-git-url=git@example.test:craft-kde.git"])
            let blueprintRoot = try current.craftBlueprintRoot(
                at: file,
                target: target,
                craftMasterDirectory: craftMasterDirectory,
                craftTargetURL: craftTargetURL
            )

            #expect(blueprintRoot.path == targetRoot.path)
            #expect(try current.blueprintOverrides(target: target, blueprintRoot: blueprintRoot) == nil)
            #expect(try legacy.blueprintOverrides(target: target, blueprintRoot: blueprintRoot) != nil)
        }
    }

    @Test
    func buildFallsBackWhenSettingsAreAbsent() throws {
        try withConfiguration("[macos-clang-arm64]\nGeneral/ABI = macos-clang-arm64\n") { file, craftMasterDirectory in
            let build = try Build.parse([])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.kdeBlueprintsRevision == nil)
            #expect(settings.clientBlueprintsRevision == nil)
            #expect(settings.compileBuildType == "RelWithDebInfo")
            #expect(settings.buildType == "RelWithDebInfo")
            #expect(!settings.buildTests)
            #expect(!settings.workOffline)
        }
    }

    @Test
    func rejectsInvalidBoolean() throws {
        try withConfiguration("[BlueprintSettings]\nnextcloud-client.buildTests = maybe\n") { file, craftMasterDirectory in
            #expect(throws: MacCrafterError.self) {
                try readConfiguration(at: file, target: target, craftMasterDirectory: craftMasterDirectory)
            }
        }
    }

    @Test
    func rejectsMalformedConfiguration() throws {
        try withConfiguration("Compile/BuildType = Debug\n") { file, craftMasterDirectory in
            let error = #expect(throws: MacCrafterError.self) {
                try readConfiguration(at: file, target: target, craftMasterDirectory: craftMasterDirectory)
            }

            #expect(error?.description.contains("MissingSectionHeaderError") == true)
        }
    }
}
