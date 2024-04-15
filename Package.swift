// swift-tools-version: 5.10
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "NextcloudFileProviderKit",
    platforms: [
        .iOS(.v14),
        .macOS(.v11),
    ],
    products: [
        // Products define the executables and libraries a package produces, making them visible to other packages.
        .library(
            name: "NextcloudFileProviderKit",
            targets: ["NextcloudFileProviderKit"]),
    ],
    dependencies: [
        .package(url: "https://github.com/nextcloud/NextcloudKit", .upToNextMajor(from: "2.9.9")),
        .package(url: "https://github.com/realm/realm-swift.git", .upToNextMajor(from: "10.33.0")),
    ],
    targets: [
        // Targets are the basic building blocks of a package, defining a module or a test suite.
        // Targets can depend on other targets in this package and products from dependencies.
        .target(
            name: "NextcloudFileProviderKit",
            dependencies: [
                .product(name: "NextcloudKit", package: "NextcloudKit"),
                .product(name: "RealmSwift", package: "realm-swift")]),
        .testTarget(
            name: "NextcloudFileProviderKitTests",
            dependencies: ["NextcloudFileProviderKit"]),
    ]
)
