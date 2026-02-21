# Xcode Workspace

**tl;dr:** Open "[Nextcloud Desktop Client.xcworkspace](../Nextcloud%20Desktop%20Client.xcworkspace/)", select "NextcloudDev" scheme and hit `⌘ + R`.

The Nextcloud desktop client project comes with an [Xcode workspace](https://developer.apple.com/documentation/xcode/managing-multiple-projects-and-their-dependencies) prepared for contributors on macOS.
Xcode is the default integrated developer environment on macOS provided by Apple.
Because our multi-platform Qt app contains native extensions for macOS, we need to build their Objective-C and Swift code through an Xcode project.
The build products are then copied into the main app which is built independently from Xcode.
Hence Xcode becomes inevitable in the daily workflow anyway, especially in regard to debugging file provider extensions.
This is the reason why this project is set up with Xcode as the primary working environment on macOS.

The workspace is an umbrella for various Swift packages and an Xcode project with different targets.
All beside the platform-independent C++ code for the main app.
The packages and the Xcode project for the macOS app extensions are meant to remain functionally independent from the workspace, though.
They have dedicated documentation to explain them in detail.
Consider the workspace a higher level organization above projects and packages.

Further, this document provides some generic developer knowledge about the typical workflow on macOS.

## Project navigator

The workspace is set up to contain references to the most important files, directories, packages and projects.
Especially platform-specific code for other platforms has been left out to focus on content relevant on macOS.

## Build artifacts

To isolate working environments and simplify cache busting, build artifacts and derived data are stored in unconventional locations.
Also, this sometimes helps to resolve KDE Craft build errors which are caused by too long file paths.

### Xcode

The Xcode workspace defines the `.xcode` directory in the repository clone root as the location for its derived data which usually is located elsewhere by default.

### Mac Crafter

The mac-crafter is called with the `.mac-crafter` directory in the repository clone root as the location for its build data which usually is located elsewhere by default. See [Craft.sh](./../shell_integration/MacOSX/NextcloudIntegration/NextcloudDev/Craft.sh).

In some occassions KDE Craft dependencies failed to build due to too long path names.
This top-level location helps to reduce that risk while maintaining isolation to other repositories on a Mac.

### Cleanup

When you have no build running, it definitely is safe to remove the previously mentioned directories.
Sometimes this is necessary to resolve build errors caused by outdated intermediate artifacts.
