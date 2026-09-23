<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Documentation asset capture

Connect NextcloudDev to the intended test server, let it sync, and quit all
Nextcloud clients. From this checkout, run:

```sh
./test/docassets/run.sh
```

No arguments or environment variables are required. The script expects
`/Applications/NextcloudDev.app/Contents/MacOS/NextcloudDev`. The capture tool
reads the server and user from the existing `nextclouddev.cfg` and reuses its
saved login. The profile must contain exactly one account. You are responsible
for ensuring it is the intended test account.

The tool locates the configuration in the NextcloudDev preferences directory,
including the macOS application container. It refuses ambiguous duplicate
profiles. It does not provision another account or copy credentials.

Assets are always exported by the script to a new
`~/Downloads/Nextcloud-doc-assets-<UTC time>-<UUID>` folder. Completed assets
from a partially failed export are retained in a folder containing
`-incomplete-`.

Each run also saves terminal diagnostics to `~/Downloads/Nextcloud-doc-assets-log-*`.
The script prints the log path before building. Attach that file when reporting
a capture failure; it can contain account names, server addresses and share data.

## Build prerequisite

The script finds the configured CMake build under this checkout's `build`
directory and builds the standalone `DocAssetsCapture` target. Build
NextcloudDev from this checkout first if no configured build exists.
The capture target must use NextcloudDev branding to read the same configuration
and Keychain credentials. The installed application alone does not contain
the capture executable.

## Screens and test data

The capture target is a separate `DocAssetsCapture.app` in the build directory.
Its bundle identity is required by the macOS notification API used during account
startup. The script checks notification initialization before exporting. Always
use `run.sh`; an older loose `bin/DocAssetsCapture` executable may still exist
after upgrading the build and must not be used.

The catalogue in `test/docassets/catalogue.cpp` covers wizard states, Settings,
Activities, Search, Sharing, User Status, and Assistant. Wizard states use local
fixtures. Account screens open the production windows with the saved account.

For Sharing, the capture reads the first page of existing server shares (up to
100), selects a file source, and opens the production sharing window using its
server file ID and display name. No local file or classic sync folder is needed.
The server must support unified sharing and have an existing file share.
The capture does not create or edit shares. Search uses the term
`Project`. The account needs representative activities and enabled Search,
User Status features. Assistant is captured only when enabled for the account;
otherwise it is reported as skipped and does not fail the export.
Missing data is reported for the affected
capture. Sharing discovery works independently of File Provider domain access.

The exporter also renders 18 bundled sync status and tray icons as PNG files.
Browser login pages, OS file pickers, and OS File Provider indicators are
outside the catalogue.

## Runtime behavior

Account captures start the real application with the existing NextcloudDev
profile. Normal startup, configuration writes, account requests, synchronization,
and platform integration can therefore occur. This is reuse of the dev client
profile, not an isolated disposable profile. Use a test account.

The normal client must be stopped because the macOS single instance socket
is shared. Capture workers block physical input and external HTTP/HTTPS URL
opening while taking screenshots.

## Validation

The standalone `DocAssetsCaptureTest` target checks notification initialization in
the bundled runner and covers configuration loading,
invalid and ambiguous accounts, server/user matching, wizard capture and icons.
Enable it with `BUILD_DOC_ASSETS_TESTS=ON` in the configured build.
A complete live export requires the prepared test account and a logged-in
macOS desktop session. Inspect the output for clipping, correct data, and
native control rendering; successful PNG decoding is not visual validation.
