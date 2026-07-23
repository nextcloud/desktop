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

Some applications like Microsoft Office, LibreOffice, Adobe, AutoCAD and Affinity by Canva create hidden lock files in the same directory a file opened by them is located in.
Office and LibreOffice lock files usually equal the name of the opened file with prefixes like `~$` or suffixes like `#`.
Adobe applications (InDesign, InCopy, Premiere Pro) use dedicated lock file extensions (`.idlk`, `.prlock`) that carry only the document's base name — not its extension — so the guarded document is resolved by matching a sibling file.
AutoCAD creates `.dwl` and `.dwl2` lock files that share the document's base name, so the guarded `.dwg` document is resolved by replacing the extension.
Affinity apps (Photo, Designer, Publisher) create lock files with a `~lock~` suffix (e.g., `Screenshot.af~lock~`); the document is resolved by stripping the suffix.
These are recognized by the file provider extension and not synchronized to the server.
However, the capabilities of the `files_lock` server app are used to lock the file for editing remotely on the server.

- ``isLockFileName(_:)``
- ``originalFileName(fromLockFileName:dbManager:)``
- ``lockFileTargetName(forLockFileName:parentServerUrl:dbManager:)``

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
