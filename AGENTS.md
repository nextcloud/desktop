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
- Never state anything in a pull request description, commit message, or review comment that is not verifiable. Every claim about what the change does, what was tested, or why it is needed must be backed by the actual diff, real test runs, or a cited source. Do not assert that tests pass, a bug is fixed, or behavior works unless it has been confirmed. If something is unverified, say so explicitly rather than presenting it as fact.

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

### License headers

Every new file must include the correct SPDX license header. For GPL-2.0-or-later (the default for this repository):

```plaintext
SPDX-FileCopyrightText: <YEAR> Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
```
The commenting signs need to be used depending on the file type.

See [HowToApplyALicense.md](https://github.com/nextcloud/server/blob/master/contribute/HowToApplyALicense.md) for details on per-language formats. AI-generated code must not include material from sources incompatible with AGPL-2.0-or-later.

Avoid creating source files that implement multiple types; instead, place each type in its own dedicated source file.

### Documentation comments

Documentation must be trustworthy: only write a comment you can back with the implementation in front of you. **A wrong or overreaching comment is worse than none** - when a behaviour is unclear or you cannot state it with confidence, leave it undocumented. This is the same verifiability rule that governs commit messages and PR text, applied to code comments.

- **Style.** Use Doxygen `/** @brief ... */` blocks for types and their members - the established convention across this codebase. Use a trailing `//!< ...` for a single instance variable or field, and plain `//` for free helper functions and file-local statics.
- **Document the type and each declared member.** For a type, state what it is and how it behaves. For each public property, method or protocol callback, state what it does. Add `@param` entries only where the meaning is not obvious from the name - in particular, spell out what `nil`, `NO`, an empty string or `0` does, and which event triggers a callback block.
- **Do not overreach.** Describe what the code actually does, not what it looks like it should do. Avoid absolute claims the implementation does not guarantee - for example, do not write "keeps the window on screen" for a helper that only best-effort clamps and can still overflow, or "loads a remote URL" for one that only reads local files.
- **Overload sets.** When a type has many overloaded initializers or methods that funnel into one, fully document the designated one (with its `@param` list) and simply mark the rest as convenience overloads rather than repeating the text.
- **Comment members selectively.** Document instance variables and file-local statics whose purpose is not obvious from their name and type; leave self-explanatory ones (a backing `_stack`, a counter) uncommented to avoid noise. In Objective-C(++), instance variables live in the `@implementation` block, so their comments belong there, not in the header.
- **Verify before you trust it.** Re-read the implementation and confirm every comment is literally true before considering the work done.

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

## C++ Specifics

The following details are important and only relevant when working on the desktop client parts written in C++ language.

You should never use `std::iostream` for input or output handling and rather use Qt's `QLoggingCategory` and related classes.

All custom logging categories starts with our "nextcloud." prefix.

Our C++ code is using a pattern known as almost always auto. You should almost never use explicit data types but the `auto` keyword for type declarations.

Our C++ code should can make use of C++ 20 standard features whenever possible.

Do not use C++ modules. Use standard header inclusion instead.

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

#### Code organization

These conventions apply to the C++, Objective-C and Objective-C++ (and their Qt-facing) code under `./src`, and macOS code under `src/gui/macOS` in particular. Follow them both when adding code and when a file has grown into an unmaintainable catch-all that must be split.

- **One type per file.** Each Objective-C class or C++ type gets its own dedicated header and implementation pair (for example `ncactionrow.h` + `ncactionrow.mm`). Never accumulate several classes in a single translation unit. Objective-C class file names are the class name lowercased, without the `NC`/type prefix stripped (`NCActionRow` → `ncactionrow`).
- **Keep the public boundary C++/Qt-only.** The seam between a macOS module and the rest of the app must expose only C++ and Qt types (for example the `showMacOSTrayPopup(const QRect &)` entry point declared in `systray.h`). *Internal* Objective-C++ headers, by contrast, may freely declare Objective-C classes and expose `NS*` types — see `fileproviderxpc_mac_utils.h` — because their only consumers are other Objective-C++ files in the same module.
- **Group a feature's files in a subdirectory.** When a split produces more than a handful of files, place them in a feature subdirectory (for example `src/gui/macOS/trayaccountpopup/`), mirroring how `src/gui` already uses `tray/`, `wizard/` and `integration/`.
- **Collect cohesive free helpers and constants into clearly-scoped support files.** Shared free functions belong in a namespaced `*utils` header/implementation pair (for example `namespace OCC::Mac::TrayPopupViewUtils`, following the existing `OCC::Mac::FileProviderXPCUtils`); shared layout constants belong in a `*metrics` header. Lightweight declarations such as a single block alias or a constant do **not** each need their own file — the one-type-per-file rule targets classes, not aliases.
- **Keep single-use helpers next to their sole user.** A file-static helper or cache used by exactly one class stays in that class's implementation file rather than being promoted to a shared header.
- **Objective-C instance variables live in the `@implementation` block**, not the header — this is the project's Objective-C equivalent of the PIMPL pattern and keeps a type's storage private to its implementation.
- **Remove dead code as part of the move.** When reorganizing an existing file, drop functions and members that are defined but never referenced instead of carrying them along.

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
