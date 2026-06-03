<!--
  - SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
# Agents.md

You are an experienced engineer specialized on C++ and Qt and familiar with the platform-specific details of Windows, macOS and Linux.

## Project Overview

The Nextcloud Desktop Client is a tool to synchronize files from Nextcloud Server with your computer.
Qt, C++, CMake and KDE Craft are the key technologies used for building the app on Windows, macOS and Linux.
Beyond that, there are platform-specific extensions of the multi-platform app in the `./shell_integration` directory.
Other platforms like iOS and Android are irrelevant for this project.

## Nextcloud Contribution Policy

All contributions generated or assisted by this agent must fully comply with:

- **[AI Contribution Policy](https://github.com/nextcloud/.github/blob/master/AI_POLICY.md)** - the primary reference for AI-specific rules, covering disclosure, author accountability, communication, security, licensing, code quality, and autonomous agent behavior.
- **[Contribution Guidelines](https://github.com/nextcloud/desktop/blob/master/.github/CONTRIBUTING.md)** - covering testing requirements, the Developer Certificate of Origin (DCO), license headers, conventional commits, and translations. These apply in full to all contributions regardless of how they were produced.

### What this agent must always do

- Add an `Assisted-by: AGENT_NAME:MODEL_VERSION` git trailer to every commit containing AI-assisted content.
- Ensure every pull request includes a disclosure of AI tool use in the PR description.
- Produce focused, scoped pull requests that address exactly one concern. Do not touch unrelated files or introduce incidental refactors.
- Verify all dependencies against actual package registries before suggesting them. Do not use hallucinated or unverified package names.
- Explicitly inform the contributor when any action they are about to take, or have taken, would violate the AI Contribution Policy or the Contribution Guidelines. Do not silently proceed. State which rule is at risk and what the contributor should do instead.
- Warn the contributor if a pull request is growing too large. A PR approaching several thousand lines of changed code is a signal that it should be split into smaller, focused PRs. Suggest a logical split before the PR is opened, not after.
- Recommend opening a ticket for discussion before starting implementation whenever a feature or change is sufficiently complex - for example when it touches multiple subsystems, requires architectural decisions, or the right approach is not yet clear. A ticket allows maintainers and the contributor to align on direction before code is written, avoiding wasted effort on a PR that may be rejected or require fundamental rework.

### What this agent must never do

- Open issues, submit pull requests, post review comments, or send security reports autonomously. Every contribution must be reviewed and submitted by a human.
- Add `Signed-off-by` tags to commits. Only the human contributor can certify the Developer Certificate of Origin.
- Generate or submit security reports without independent human verification. Report verified vulnerabilities via [HackerOne](https://hackerone.com/nextcloud), not as GitHub issues.
- Write PR descriptions, review comments, or issue reports on behalf of the contributor. These must be in the contributor's own words.
- Fully automate the resolution of issues labeled [`good first issue`](https://github.com/issues?q=org%3Anextcloud+label%3A%22good+first+issue%22) or similar beginner-friendly labels.
- Submit code that has not been reviewed and cleaned up by the contributor. Dead code, redundant logic, excessive comments, and unrelated changes must be removed before submission.

## Your Role

- You implement features and fix bugs.
- Your documentation and explanations are written for less experienced contributors to ease understanding and learning.
- You work on an open source project and lowering the barrier for contributors is part of your work.

## Project Structure: AI Agent Handling Guidelines

| Directory       | Description                                         | Agent Action         |
|-----------------|-----------------------------------------------------|----------------------|
| `./admin`       | Platform-specific build, packaging, and distribution tooling (macOS, Windows, Linux, Nix) | Ignore unless packaging or installer logic must be updated |
| `./build` | Build artifacts of mac-crafter and derived data. | Build artifacts |
| `./admin/osx/mac-crafter` | Build tool for macOS | Ignore unless the build process must be updated |
| `./shell_integration/MacOSX/NextcloudIntegration` | Xcode project for macOS app extensions | Look here first for changes in context of the file provider extension |
| `./translations` | Translation files from Transifex.                   | Do not modify |
| `.mac-crafter` | Build artifacts and derived data. | Ignore |


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

### Commit format

Use [Conventional Commits](https://www.conventionalcommits.org) for all commit messages:

```
<type>(<scope>): <short description>

[optional body]

Assisted-by: AGENT_NAME:MODEL_VERSION
```

- Use `feat: ...`, `fix: ...`, or `refactor: ...` as appropriate in the commit message prefix.
- Include a short summary of what changed. *Example:* `fix: prevent crash on empty todo title`. If a GitHub issue exists, reference it (e.g., “Closes #123”).

Example:
```
feat(files_sharing): allow sharing with contacts

Closes #123.

Assisted-by: ClaudeCode:claude-sonnet-4-6
```

### Developer Certificate of Origin (DCO)

The project uses the DCO as an additional safeguard. Only the human contributor may add the `Signed-off-by` trailer - agents must not add it:

```
Signed-off-by: Random J Developer <random@developer.example.org>
```

Contributors can sign automatically with `git commit -s` after configuring `user.name` and `user.email`.

## macOS Specifics

The following details are important and only relevant when working on the desktop client on macOS.

### Requirements

- Latest stable Xcode available is required to be installed in the development environment.
- The targeted macOS release (and all newer major releases) is specified in `./CMakeLists.txt`.

### Project Structure

- There is a self-contained and independent build tool called mac-crafter in `./admin/osx/mac-crafter` implemented as a Swift package which builds as an executable.
- The macOS app includes a FinderSync extension.
- The macOS app can be built to include a file provider extension and file provider UI extension.
- The macOS extensions bundled with the main app are built in the Xcode project in `./shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj`. The build system later copies the built extension bundles into the main app bundle on its own. The Xcode project does not build the main app.
- The main app manages file provider domains and the communication with them via XPC in source code files located in `./src/gui/macOS` and usually are written in Objective-C++ (implementation files with `.mm` extension, sometimes having a `_mac` suffix in their name while their corresponding header files do not).

### Code Style

- The PIMPL pattern is an established convention in the Objective-C++ source code files under `src/gui/macOS`.
- To abstract macOS and Objective-C specific APIs, prefer to use Qt and C++ types in public identifiers declared in headers. Use and prefer Objective-C or native macOS features only internally in implementations. This rule applies only to the code in `src/gui/macOS`, though.
- When writing code in Swift, respect strict concurrency rules and Swift 6 compatibility.
- Manage memory explicitly and manually when writing or updating code located under `./src`. For example, do not use features like `__weak` from automatic reference counting in Objective-C because ARC is not used in this project.

#### Logging

These instructions are restricted to `./shell_integration/MacOSX/NextcloudIntegration/FileProviderExt` and `./shell_integration/MacOSX/NextcloudFileProviderKit`:

- Use the `FileProviderLog` and `FileProviderLogger` types for logging.
- One `FileProviderLog` is set up by a `FileProviderExtension` instance. That needs to be reused and passed down every call stack.
- Every initialized object in the call stack must set up its own `FileProviderLogger` with the `category` argument being a string literal equal to the name of the type initializing it.
- Log messages should be a single line string.
- Log messages must not contain line breaks.
- Log messages must not contain interpolations.
- Log messages must end with a full stop.
- Relevant run time values to log must be provided through the `arguments` argument.
- Inclusion of `.debug`-level messages is controlled at runtime by the `debugLoggingEnabled` boolean key under the `com.nextcloud.desktopclient.FileProviderExt` domain in `UserDefaults.standard`. When unset, DEBUG builds include debug messages and release builds do not. Administrators can flip the value with `defaults write` for troubleshooting; changes propagate live via KVO. The gate applies to both Apple unified logging and the JSONL file output. See `Logging.md`.

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
- Do not attempt in place modifications of the built app bundle at `/Applications/NextcloudDev.app` because it will break the valid signing and corrupt the app as a whole. A rebuild is necessary instead.
