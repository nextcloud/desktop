// SPDX-FileCopyrightText: 2024 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import PackageDescription

let package = Package(
    name: "mac-crafter",
    platforms: [
        .macOS(.v11),
    ],
    dependencies: [
        .package(url: "https://github.com/apple/swift-argument-parser", from: "1.4.0")
    ],
    targets: [
        // Targets are the basic building blocks of a package, defining a module or a test suite.
        // Targets can depend on other targets in this package and products from dependencies.
        .executableTarget(
            name: "mac-crafter",
            dependencies: [.product(name: "ArgumentParser", package: "swift-argument-parser")]
        ),
    ]
)
