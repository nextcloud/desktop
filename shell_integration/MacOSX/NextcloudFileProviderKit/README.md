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
