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

## License

This project is licensed under the LGPLv3 License. See the [LICENSE](LICENSE) file for more details.

---
