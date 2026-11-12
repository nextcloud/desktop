<!--
SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

<div align="center">
    <img src="NextcloudFileProviderKit.svg" alt="Logo of NextcloudFileProviderKit" width="256" height="256" />
</div>

# NextcloudFileProviderKit

NextcloudFileProviderKit is a Swift package designed to simplify the development of Nextcloud synchronization applications on Apple devices using the [File Provider Framework](https://developer.apple.com/documentation/FileProvider). This package provides the core functionality for virtual files in the macOS Nextcloud client, making it easier for developers to integrate Nextcloud syncing capabilities into their applications.

NextcloudFileProviderKit depends on NextcloudKit to communicate with the server.

## Features

- **Easy Integration**: Seamlessly integrate Nextcloud syncing into your Apple applications using the FileProvider API.
- **Core Functionality**: Provides the essential features needed for handling virtual files, including fetching contents, creating, modifying, and deleting items.
- **macOS Support**: Used as the core functionality package for virtual files in the macOS Nextcloud client.

## Installation

To install NextcloudFileProviderKit, add the following to your `Package.swift`:

```swift
dependencies: [
    .package(url: "https://github.com/nextcloud/NextcloudFileProviderKit.git", from: "1.0.0")
]
```

## Usage

This section has been removed due to being out of dated and frequent changes to the code.
As a reference, you can have a look at [this Xcode project](https://github.com/nextcloud/desktop/tree/master/shell_integration/MacOSX/NextcloudIntegration) in the Nextcloud desktop client.
There are also plans to make this package more self-contained than it currently is and some code will be migrated from the other project to this one.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue if you encounter any problems or have suggestions for improvements.

### Code Style

[SwiftFormat](https://github.com/nicklockwood/SwiftFormat) was introduced into this project.
Before submitting a pull request, please ensure that your code changes comply with the currently configured code style.
You can run the following command in the root of the package repository clone:

```bash
swift package plugin --allow-writing-to-package-directory swiftformat --verbose --cache ignore
```

### Performance tests

The `NextcloudFileProviderKitPerformanceTests` target counts how much work this package does, without a server or any setup.
The production `Enumerator` and a real database run in the test process against `MockRemoteInterface`, which stands in for the server.

Each scenario counts four things per pass:

- **requests:** PROPFIND requests sent to the server
- **commits:** database transactions committed
- **row changes:** database rows inserted, updated or deleted
- **reported:** items reported to the system

The counts are the same on every machine and in every build configuration, so a claim that a change does less work is held to them.
The tests time nothing.

#### What CI checks

CI runs the target on every pull request.
Each scenario checks that every pass did the same work and that no count is above its ceiling in `EnumerationBenchmarks.swift`.

- If your change makes a count go up, CI fails. Raise the ceiling in the same pull request and explain why in its description.
- If your change makes a count go down, lower the ceiling to the new value. The diff then shows the numbers before and after.

#### Adding a scenario

Write a `@Test` in the target that calls `Benchmark.measure` with a `prepare` that sets up one pass, a `run` that returns the number of items it reported, and a `cleanUp` that removes its `TemporaryDatabase`.
Check the counts it returns with `expectWithin`.
To find the ceilings, start with zeros and run `swift test --filter NextcloudFileProviderKitPerformanceTests`. Each failing expectation shows the count to put in its place.
For working-set scenarios, start from `BrowsedTreeFixture`.

#### What is not measured

Automated performance tests are only useful when they are reproducible, and only what this package controls can be reproduced.
Network conditions, server response times and how `fileproviderd`, the file provider daemon of macOS, takes in reported items are outside its control. They also differ from one run to the next, so the tests leave them out on purpose.

The server is a mock in the same process, so there is no HTTP, no XML parsing, no network latency and no server work.
The daemon is not involved either. Its work on the reported items is missing, and so is how often it asks for an enumeration.
Network cost shows in the request count, which holds for any server. The daemon's cost shows only in the reported count.
Listings paged by the server (Nextcloud 31 and later) are covered for one page only. For a second page this package sends its encoded page cursor as the server's token, which the mock rejects.

#### Claiming that users wait less

The counts show that a change does less work, not how much sooner users see the result.
If your pull request claims that users wait less, measure the installed app, built in release, against a real server, before and after your change.
Nobody can repeat that measurement exactly, so describe it well enough for a reviewer to check it: client and server versions, folder sizes, network, how many runs you made and how much they varied.

### Localization

Transifex is used for localization.
These localizations are excluded from our usual and automated translation flow due to how Transifex synchronizes Xcode string catalogs and the danger of data loss.
To pull updated localizations from Transifex into the Xcode project manually, follow the steps below.

#### Configuration

The dedicated [`.tx/config`](.tx/config) file is used.

### Pulling Translations

Run this in the "NextcloudFileProviderKit" package folder of your repository clone:

```sh
tx pull --force --all --mode=translator
```

The `translator` mode is important here and for later, so unreviewed strings are included and not accidentally deleted.
See [the official Transifex documentation on Xcode string catalogs and download modes](https://help.transifex.com/en/articles/9459174-xcode-strings-catalogs-xcstrings#h_786f60d73b).
Do not commit the changes string catalogs, they need to be processed first.

#### Sanitize Translations

Transifex returns empty strings for keys with untranslated localizations.
To remove them, we use the Swift command-line utility [TransifexStringCatalogSanitizer](../TransifexStringCatalogSanitizer/).
See its dedicated README for usage instructions.
Use it for all updated Xcode string catalogs.

#### Summary

```sh
tx pull --force --all --mode=translator
swift run --package-path=../TransifexStringCatalogSanitizer TransifexStringCatalogSanitizer ./Sources/NextcloudFileProviderKit/Resources/Localizable.xcstrings
```

### Pushing Translations

**Follow this section carefully to the end before performing any steps of it.**
The way Transifex handles Xcode string catalogs poses a high risk of accidentally deleting already available and finished translations on Transifex because pushing an Xcode string catalog overwrites the online state with the catalog as it is.
This means that changes on Transifex must be integrated locally first to avoid data loss, before the then updated local Xcode string catalog can be pushed to Transifex.

1. Perform the steps in the previous section about pulling translations.
2. Build the extensions in Xcode. This causes the compiler to update the string catalogs based on the current source code by automatically recognizing localizable texts. As of writing, the "desktopclient" scheme is a good choice because it builds both file provider extensions as dependencies.
3. Run the `TransifexStringCatalogSanitizer` over both string catalogs as in the previous section.
4. Inspect the changes in the string catalogs in a Git diff or whatever tool you use for that task. Verify the plausibility of each change.
5. Run `tx push` in the "NextcloudIntegration" directory.
6. Check Transifex to have received the new keys and deleted the obsolete ones.

## License

This project is licensed under the LGPLv3 License. See the [LICENSE](LICENSE) file for more details.

---
