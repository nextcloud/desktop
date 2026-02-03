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

## Project Structure: AI Agent Handling Guidelines

| Directory       | Description                                         | Agent Action         |
|-----------------|-----------------------------------------------------|----------------------|
| `./admin/osx/mac-crafter` | Build tool for macOS | Ignore unless the build process must be updated |
| `./shell_integration/MacOSX/NextcloudIntegration` | Xcode project for macOS app extensions | Look here first for changes in context of the file provider extension |
| `./translations` | Translation files from Transifex.                   | Do not modify        |

## General Guidance

Every new file needs to get a SPDX header in the first rows according to this template. 
The year in the first line must be replaced with the year when the file is created (for example, 2026 for files first added in 2026).
The commenting signs need to be used depending on the file type.

```plaintext
SPDX-FileCopyrightText: <YEAR> Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
```

## Commit and Pull Request Guidelines

- **Commits**: Follow Conventional Commits format. Use `feat: ...`, `fix: ...`, or `refactor: ...` as appropriate in the commit message prefix.
- Include a short summary of what changed. *Example:* `fix: prevent crash on empty todo title`.
- **Pull Request**: When the agent creates a PR, it should include a description summarizing the changes and why they were made. If a GitHub issue exists, reference it (e.g., “Closes #123”).

## macOS Specifics

The following details are important when working on the desktop client on macOS.

- Latest stable Xcode available is required to be installed in the development environment.
- There is a self-contained and independent build tool called mac-crafter in `./admin/osx/mac-crafter` implemented as a Swift package which builds as an executable.
- To enable a macOS app build, the file `./shell_integration/MacOSX/NextcloudIntegration/NextcloudDev/Build.xcconfig` must be created if not existent already and it must contain the Xcode build setting `CODE_SIGN_IDENTITY=Apple Development`.
- To verify that the project builds successfully on macOS, mac-crafter can be run in its own directory with these arguments: `swift run mac-crafter --build-path=DerivedData --product-path=/Applications --build-type=Debug --dev --disable-auto-updater --build-file-provider-module`
- The macOS app includes a FinderSync extension.
- The macOS app can be built to include a file provider extension and file provider UI extension.
- The macOS extensions bundled with the main app are built in the Xcode project in `./shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj`. The build system later copies the built extension bundles into the main app bundle on its own. The Xcode project does not build the main app.
- The main app manages file provider domains and the communication with them via XPC in source code files located in `./src/gui/macOS` and usually are written in Objective-C++ (implementation files with `.mm` extension, sometimes having a `_mac` suffix in their name while their corresponding header files do not). The PIMPL pattern is an established convention here.
- When writing code in Swift, respect strict concurrency rules and Swift 6 compatibility.