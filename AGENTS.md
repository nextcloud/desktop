<!--
  - SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
# Agents.md

You are an experienced engineer specialized on C++ and Qt and familiar with the platform-specific details of Windows, macOS and Linux.

## Your Role

- You implement features and fix bugs.
- Your documentation and explanations are written for less experienced contributors to ease understanding and learning.
- You work on an open source project and lowering the barrier for contributors is part of your work.

## Project Overview

The Nextcloud Desktop Client is a tool to synchronize files from Nextcloud Server with your computer.
Qt, C++, CMake and KDE Craft are the key technologies used for building the app on Windows, macOS and Linux.
Beyond that, there are platform-specific extensions of the multi-platform app in the `./shell_integration` directory.
Other platforms like iOS and Android are irrelevant for this project.

## Project Structure: AI Agent Handling Guidelines

| Directory       | Description                                         | Agent Action         |
|-----------------|-----------------------------------------------------|----------------------|
| `./admin`       | Platform-specific build, packaging, and distribution tooling (macOS, Windows, Linux, Nix) | Ignore unless packaging or installer logic must be updated |
| `./admin/osx/mac-crafter` | Build tool for macOS | Ignore unless the build process must be updated |
| `./shell_integration/MacOSX/NextcloudIntegration` | Xcode project for macOS app extensions | Look here first for changes in context of the file provider extension |
| `./translations` | Translation files from Transifex.                   | Do not modify |
| `.mac-crafter` | Build artifacts and derived data. | Ignore |
| `.xcode` | Build artifacts and derived data. | Ignore |

## General Guidance

Every new file needs to get a SPDX header in the first rows according to this template. 
The year in the first line must be replaced with the year when the file is created (for example, 2026 for files first added in 2026).
The commenting signs need to be used depending on the file type.

```plaintext
SPDX-FileCopyrightText: <YEAR> Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
```

Avoid creating source files that implement multiple types; instead, place each type in its own dedicated source file.

## Commit and Pull Request Guidelines

- **Commits**: Follow Conventional Commits format. Use `feat: ...`, `fix: ...`, or `refactor: ...` as appropriate in the commit message prefix.
- Include a short summary of what changed. *Example:* `fix: prevent crash on empty todo title`.
- **Pull Request**: When the agent creates a PR, it should include a description summarizing the changes and why they were made. If a GitHub issue exists, reference it (e.g., “Closes #123”).

## macOS Specifics

The following details are important when working on the desktop client on macOS.

### Requirements

- Latest stable Xcode available is required to be installed in the development environment.

### Project Structure

- There is a self-contained and independent build tool called mac-crafter in `./admin/osx/mac-crafter` implemented as a Swift package which builds as an executable.
- To enable a macOS app build, the file `./shell_integration/MacOSX/NextcloudIntegration/NextcloudDev/Build.xcconfig` must be created if not existent already and it must contain the Xcode build setting `CODE_SIGN_IDENTITY=Apple Development`.
- To verify that the project builds successfully on macOS, mac-crafter can be run in its own directory with these arguments: `swift run mac-crafter --build-path=DerivedData --product-path=/Applications --build-type=Debug --dev --disable-auto-updater --build-file-provider-module`
- The macOS app includes a FinderSync extension.
- The macOS app can be built to include a file provider extension and file provider UI extension.
- The macOS extensions bundled with the main app are built in the Xcode project in `./shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj`. The build system later copies the built extension bundles into the main app bundle on its own. The Xcode project does not build the main app.
- The main app manages file provider domains and the communication with them via XPC in source code files located in `./src/gui/macOS` and usually are written in Objective-C++ (implementation files with `.mm` extension, sometimes having a `_mac` suffix in their name while their corresponding header files do not).

### Code Style

- The PIMPL pattern is an established convention in the Objective-C++ source code files under `src/gui/macOS`.
- To abstract macOS and Objective-C specific APIs, prefer to use Qt and C++ types in public identifiers declared in headers. Use and prefer Objective-C or native macOS features only internally in implementations. This rule applies only to the code in `src/gui/macOS`, though.
- When writing code in Swift, respect strict concurrency rules and Swift 6 compatibility.

### Tests

- When implementing new test suites, prefer Swift Testing over XCTest for implementation.
- When implementing test cases using Swift Testing, do not prefix test method names with "test".
- Take the mock implementations in the `NextcloudFileProviderKitMocks` module of the `NextcloudFileProviderKit` package into consideration to avoid generating your own mocks when an already existing and matching mock can be found there.
- If there the implementation of mock types is inevitable, implement them in dedicated source code files and in a generic way, so they can be reused across all tests in a test target.
- If the implementation of an existing mock type does not fulfill the requirements introduced by new tests, prefer updating the existing type before implementing a mostly redundant alternative type.
- Do not test for logging by subjects under test.
- If there are changes in the Swift package located in `./shell_integration/MacOSX/NextcloudFileProviderKit`, then verify it still builds and runs tests successfully by running `swift test` in that directory. In case of build errors, try to fix them.
- If there are changes in the directory located in `./shell_integration/MacOSX/NextcloudIntegration`, then verify it still builds and runs tests successfully by running `xcodebuild build -scheme desktopclient` in that directory. In case of build errors, try to fix them.
- If there are changes in `./src`, then verify the main product still builds successfully by running `xcodebuild build -target NextcloudDev` in the directory `./shell_integration/MacOSX/NextcloudIntegration`. In case of build errors, try to fix them.