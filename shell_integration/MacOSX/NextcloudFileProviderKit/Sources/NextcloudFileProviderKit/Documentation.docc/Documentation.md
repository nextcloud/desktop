<!--
SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ``NextcloudFileProviderKit``

NextcloudFileProviderKit is a Swift package designed to simplify the development of Nextcloud synchronization applications on Apple devices using the File Provider Framework. This package provides the core functionality for virtual files in the macOS Nextcloud client, making it easier for developers to integrate Nextcloud syncing capabilities into their applications.


## Topics

### File Provider

- ``Enumerator``
- ``Item``
- ``MaterialisedEnumerationObserver``

### Lock File Support

Some applications like Microsoft Office and LibreOffice create hidden lock files in the same directory a file opened by them is located in.
They usually equal the name of the opened file with prefixes like `~$` or suffixes like `#`.
These are recognized by the file provider extension and not synchronized to the server.
However, the capabilities of the `files_lock` server app are used to lock the file for editing remotely on the server.

- ``isLockFileName(_:)``
- ``originalFileName(fromLockFileName:dbManager:)``

### Logging

This package comes with its own reusable logging solution which builds on top of the unified logging system provided by the platform and a JSON lines based file logging.
It is designed specifically for the implementation of this file provider extension.

- ``FileProviderLog``
- ``FileProviderLogDetail``
- ``FileProviderLogDetailKey``
- ``FileProviderLogger``
- ``FileProviderLogging``
- ``FileProviderLogMessage``

### Persistence

- ``FilesDatabaseManager``
- ``ItemMetadata``
- ``SendableItemMetadata``
