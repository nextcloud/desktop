/*
 * Copyright (C) 2024 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import Foundation

enum PackagingError: Error {
    case projectNameSettingError(String)
    case packageBuildError(String)
    case packageSigningError(String)
    case packageNotarisationError(String)
    case packageSparkleBuildError(String)
    case packageSparkleSignError(String)
}

/// NOTE: Requires Packages utility. http://s.sudre.free.fr/Software/Packages/about.html
fileprivate func buildPackage(appName: String, buildWorkPath: String, productPath: String) throws -> String {
    let packageFile = "\(appName).pkg"
    let pkgprojPath = "\(buildWorkPath)/admin/osx/macosx.pkgproj"

    guard shell("packagesutil --file \(pkgprojPath) set project name \(appName)") == 0 else {
        throw PackagingError.projectNameSettingError("Could not set project name in pkgproj!")
    }
    guard shell("packagesbuild -v --build-folder \(productPath) -F \(productPath) \(pkgprojPath)") == 0 else {
        throw PackagingError.packageBuildError("Error building pkg file!")
    }
    return "\(productPath)/\(packageFile)"
}

fileprivate func signPackage(packagePath: String, packageSigningId: String) throws {
    let packagePathNew = "\(packagePath).new"
    guard shell("productsign --timestamp --sign '\(packageSigningId)' \(packagePath) \(packagePathNew)") == 0 else {
        throw PackagingError.packageSigningError("Could not sign pkg file!")
    }
    let fm = FileManager.default
    try fm.removeItem(atPath: packagePath)
    try fm.moveItem(atPath: packagePathNew, toPath: packagePath)
}

fileprivate func notarisePackage(
    packagePath: String, appleId: String, applePassword: String, appleTeamId: String
) throws {
    guard shell("xcrun notarytool submit \(packagePath) --apple-id \(appleId) --password \(applePassword) --team-id \(appleTeamId) --wait") == 0 else {
        throw PackagingError.packageNotarisationError("Failure when notarising package!")
    }
    guard shell("xcrun stapler staple \(packagePath)") == 0 else {
        throw PackagingError.packageNotarisationError("Could not staple notarisation on package!")
    }
}

fileprivate func buildSparklePackage(packagePath: String, buildPath: String) throws -> String {
    let sparkleTbzPath = "\(packagePath).tbz"
    guard shell("tar cf \(sparkleTbzPath) \(packagePath)") == 0 else {
        throw PackagingError.packageSparkleBuildError("Could not create Sparkle package tbz!")
    }
    return sparkleTbzPath
}

fileprivate func signSparklePackage(sparkleTbzPath: String, buildPath: String, signKey: String) throws {
    guard shell("\(buildPath)/bin/sign_update -s \(signKey) \(sparkleTbzPath)") == 0 else {
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
) throws {
    print("Creating pkg file for client…")
    let buildWorkPath = "\(buildPath)/\(craftTarget)/build/\(craftBlueprintName)/work/build"
    let packagePath = try buildPackage(
        appName: appName,
        buildWorkPath: buildWorkPath,
        productPath: productPath
    )

    if let packageSigningId {
        print("Signing pkg with \(packageSigningId)…")
        try signPackage(packagePath: packagePath, packageSigningId: packageSigningId)

        if let appleId, let applePassword, let appleTeamId {
            print("Notarising pkg with Apple ID \(appleId)…")
            try notarisePackage(
                packagePath: packagePath,
                appleId: appleId,
                applePassword: applePassword,
                appleTeamId: appleTeamId
            )
        }
    }

    print("Creating Sparkle TBZ file…")
    let sparklePackagePath =
        try buildSparklePackage(packagePath: packagePath, buildPath: buildPath)

    if let sparklePackageSignKey {
        print("Signing Sparkle TBZ file…")
        try signSparklePackage(
            sparkleTbzPath: sparklePackagePath,
            buildPath: buildPath,
            signKey: sparklePackageSignKey
        )
    }
}
