// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing
@testable import mac_crafter

struct CraftMasterConfigurationTests {
    private let target = "macos-clang-arm64"

    private func withConfiguration(_ contents: String, validate: (URL, URL) throws -> Void) throws {
        let fileManager = FileManager.default
        let directory = fileManager.temporaryDirectory
            .appendingPathComponent("mac-crafter test \(UUID().uuidString)")
        defer { try? fileManager.removeItem(at: directory) }

        let craftMasterDirectory = directory.appendingPathComponent("craftmaster")
        try fileManager.createDirectory(at: craftMasterDirectory, withIntermediateDirectories: true)
        let fixture = try #require(Bundle.module.url(forResource: "Config", withExtension: "py"))
        try fileManager.copyItem(at: fixture, to: craftMasterDirectory.appendingPathComponent("Config.py"))

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

            #expect(settings.kdeBlueprintsRepository.hasSuffix("||stable-kde"))
            #expect(settings.clientBlueprintsRepository.hasSuffix("||6e684481b778f177944642f588e6e6f3ed95f558"))
            #expect(settings.compileBuildType == "Release")
            #expect(settings.buildType == "MinSizeRel")
            #expect(settings.buildTests)
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
                "--kde-blueprints-git-ref=custom-kde",
                "--client-blueprints-git-ref=custom-client",
                "--build-type=Debug",
                testFlag,
            ])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.kdeBlueprintsRepository.hasSuffix("|custom-kde|"))
            #expect(settings.clientBlueprintsRepository.hasSuffix("|custom-client|"))
            #expect(settings.compileBuildType == "Debug")
            #expect(settings.buildType == "Debug")
            #expect(settings.buildTests == (testFlag == "--build-tests"))
        }
    }

    @Test
    func buildFallsBackWhenSettingsAreAbsent() throws {
        try withConfiguration("[macos-clang-arm64]\nGeneral/ABI = macos-clang-arm64\n") { file, craftMasterDirectory in
            let build = try Build.parse([])
            let settings = try build.craftSettings(at: file, target: target, craftMasterDirectory: craftMasterDirectory)

            #expect(settings.kdeBlueprintsRepository.hasSuffix("|master|"))
            #expect(settings.clientBlueprintsRepository.hasSuffix("|master|"))
            #expect(settings.compileBuildType == "RelWithDebInfo")
            #expect(settings.buildType == "RelWithDebInfo")
            #expect(!settings.buildTests)
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
            #expect(throws: MacCrafterError.self) {
                try readConfiguration(at: file, target: target, craftMasterDirectory: craftMasterDirectory)
            }
        }
    }
}
