// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2024 Claudio Cambra
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

enum PackagingError: Error {
    case projectNameSettingError(String)
    case packageBuildError(String)
    case packageSigningError(String)
    case packageNotarisationError(String)
    case packageSparkleBuildError(String)
    case packageSparkleSignError(String)
    case packageCreateDmgFailed(String)
}

/// NOTE: Requires Packages utility. http://s.sudre.free.fr/Software/Packages/about.html
fileprivate func buildPackage(appName: String, buildWorkPath: String, productPath: String) async throws -> String {
    let packageFile = "\(appName).pkg"
    let pkgprojPath = "\(buildWorkPath)/admin/osx/macosx.pkgproj"

    guard await shell("packagesutil --file \(pkgprojPath) set project name \(appName)") == 0 else {
        throw PackagingError.projectNameSettingError("Could not set project name in pkgproj!")
    }
    guard await shell("packagesbuild -v --build-folder \(productPath) -F \(productPath) \(pkgprojPath)") == 0 else {
        throw PackagingError.packageBuildError("Error building package file!")
    }
    return "\(productPath)/\(packageFile)"
}

fileprivate func signPackage(packagePath: String, packageSigningId: String) async throws {
    let packagePathNew = "\(packagePath).new"
    guard await shell("productsign --timestamp --sign '\(packageSigningId)' \(packagePath) \(packagePathNew)") == 0 else {
        throw PackagingError.packageSigningError("Could not sign package file!")
    }
    let fm = FileManager.default
    try fm.removeItem(atPath: packagePath)
    try fm.moveItem(atPath: packagePathNew, toPath: packagePath)
}

fileprivate func notarisePackage(
    packagePath: String, appleId: String, applePassword: String, appleTeamId: String
) async throws {
    guard await shell("xcrun notarytool submit \(packagePath) --apple-id \(appleId) --password \(applePassword) --team-id \(appleTeamId) --wait") == 0 else {
        throw PackagingError.packageNotarisationError("Failure when notarising package!")
    }
    guard await shell("xcrun stapler staple \(packagePath)") == 0 else {
        throw PackagingError.packageNotarisationError("Could not staple notarisation on package!")
    }
}

fileprivate func buildSparklePackage(packagePath: String, buildPath: String) async throws -> String {
    let sparkleTbzPath = "\(packagePath).tbz"
    guard await shell("tar cf \(sparkleTbzPath) \(packagePath)") == 0 else {
        throw PackagingError.packageSparkleBuildError("Could not create Sparkle package tbz!")
    }
    return sparkleTbzPath
}

fileprivate func signSparklePackage(sparkleTbzPath: String, buildPath: String, signKey: String) async throws {
    guard await shell("\(buildPath)/bin/sign_update -s \(signKey) \(sparkleTbzPath)") == 0 else {
        throw PackagingError.packageSparkleSignError("Could not sign Sparkle package tbz!")
    }
}

func packageAppBundle(
    productPath: String,
    buildPath: String,
    craftTarget: String,
    craftBlueprintName: String,
    appName: String,
    packageSigningId: String?,
    appleId: String?,
    applePassword: String?,
    appleTeamId: String?,
    sparklePackageSignKey: String?
) async throws {
    Log.info("Creating package file for client…")
    let buildWorkPath = "\(buildPath)/\(craftTarget)/build/\(craftBlueprintName)/work/build"
    let packagePath = try await buildPackage(
        appName: appName,
        buildWorkPath: buildWorkPath,
        productPath: productPath
    )

    if let packageSigningId {
        Log.info("Signing package with \(packageSigningId)…")
        try await signPackage(packagePath: packagePath, packageSigningId: packageSigningId)

        if let appleId, let applePassword, let appleTeamId {
            Log.info("Notarising package with Apple ID \(appleId)…")
            try await notarisePackage(
                packagePath: packagePath,
                appleId: appleId,
                applePassword: applePassword,
                appleTeamId: appleTeamId
            )
        }
    }

    Log.info("Creating Sparkle TBZ file…")
    let sparklePackagePath =
        try await buildSparklePackage(packagePath: packagePath, buildPath: buildPath)

    if let sparklePackageSignKey {
        Log.info("Signing Sparkle TBZ file…")
        try await signSparklePackage(
            sparkleTbzPath: sparklePackagePath,
            buildPath: buildPath,
            signKey: sparklePackageSignKey
        )
    }
}

func createDmgForAppBundle(
    appBundlePath: String,
    productPath: String,
    buildPath: String,
    appName: String,
    packageSigningId: String?,
    appleId: String?,
    applePassword: String?,
    appleTeamId: String?,
    sparklePackageSignKey: String?
) async throws {
    Log.info("Creating disk image for the client…")

    let dmgFilePath = URL(fileURLWithPath: productPath)
        .appendingPathComponent(appName)
        .appendingPathExtension("dmg")
        .path

    guard await shell("create-dmg --volname \(appName) --filesystem APFS --app-drop-link 513 37 --window-size 787 276 \"\(dmgFilePath)\" \"\(appBundlePath)\"") == 0 else {
        throw PackagingError.packageCreateDmgFailed("Command failed.")
    }

    if let packageSigningId {
        Log.info("Signing disk image with \(packageSigningId)…")
        await Signer.sign(at: URL(fileURLWithPath: dmgFilePath), with: packageSigningId, entitlements: nil)

        if let appleId, let applePassword, let appleTeamId {
            Log.info("Notarising disk image with Apple ID \(appleId)…")
            
            try await notarisePackage(
                packagePath: dmgFilePath,
                appleId: appleId,
                applePassword: applePassword,
                appleTeamId: appleTeamId
            )
        }
    }

    Log.info("Creating Sparkle TBZ file…")
    let sparklePackagePath = try await buildSparklePackage(packagePath: dmgFilePath, buildPath: buildPath)

    if let sparklePackageSignKey {
        Log.info("Signing Sparkle TBZ file…")

        try await signSparklePackage(
            sparkleTbzPath: sparklePackagePath,
            buildPath: buildPath,
            signKey: sparklePackageSignKey
        )
    }
}
