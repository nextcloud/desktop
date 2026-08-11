<!--
  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  SPDX-License-Identifier: GPL-2.0-or-later
-->

# Nextcloud UI screenshots

This development-only Xcode workflow captures production Nextcloud Desktop UI for documentation. One run of the shared `Nextcloud UI Screenshots` scheme builds a standalone screenshot executable in a separate CMake tree, then starts two processes from that executable: an isolated QML capture phase followed by a native-widget capture phase. The script verifies the complete manifest before exporting it.

All screenshot-owned implementation, fixtures, tests, CMake integration, and scripts live below `tools/ui-screenshots`. The ordinary `NextcloudDev` target does not configure, compile, define, link, install, or dispatch screenshot code, and no production source file contains screenshot behavior. The isolated target replaces only the side-effecting File Provider service controller with a nonvisual fixture; the production Settings dialog and its production General and Account Settings widgets compile unchanged against the same controller API. The native fixture also reuses the existing `FolderManTestHelper` instead of widening the production `FolderMan` API.

## Safety boundary

Quit every running `Nextcloud` and `NextcloudDev` client before starting. The preparation script detects conflicting processes and fails without terminating them.

Both phases use a minimal `QApplication`; neither constructs the normal `OCC::Application`. Each process creates its own temporary configuration directory inside the staging area and removes it on normal exit. Neither process reads the user's normal Nextcloud configuration. The native phase does not restore the user's account database, start account connections, reconcile File Provider domains, configure File Provider XPC, initialize FinderSync, or show a system-tray icon.

The standalone executable is built below `build/ui-screenshots-<architecture>`. It is not added to the normal developer target, copied into `NextcloudDev.app`, or installed into `/Applications`. Its CMake cache is independent from the KDE Craft client build, so a failed screenshot build cannot leave screenshot definitions enabled for a later normal build.

The production Settings dialog still receives the small dependencies its widgets require: an empty `FolderMan` and a fictional, in-memory, signed-out account for `Alex Morgan` at `cloud.example.com`. The account is added only to that dialog, is never registered with `AccountManager`, and is discarded with the process. Connection Settings and the ignored-files editor are opened through the actual Account Settings and Advanced Settings actions. File Provider settings remain visible, but the isolated nonvisual controller reads and writes only the temporary profile and implements no system-domain or XPC operations.

The workflow therefore does not require a separate macOS user or removal of real accounts and File Provider domains. Use the desired macOS light or dark appearance; both phases intentionally use source English.

## Run from Xcode

1. Open `shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj`.
2. Select the shared `Nextcloud UI Screenshots` scheme.
3. Choose Run once.
4. Watch the Xcode report navigator or console for preparation, QML, native, or export failures.

The scheme first builds the ordinary `NextcloudDev` target so the KDE Craft dependency prefix is available. That build contains no screenshot code. Its Launch action passes `tools/ui-screenshots/run-all.sh` to `/bin/sh` without attaching LLDB. The script configures and builds only `nextcloud-ui-screenshots` in the separate build tree, starts it for QML capture, waits for successful completion, and starts it again for native capture.

Screenshots use the active system appearance and the production theme. No rendering backend, Qt style, light mode, or dark mode is forced. Both isolated phases use source English because they do not initialize the normal application translators.

The same workflow can be started from Terminal with:

```sh
/bin/sh tools/ui-screenshots/run-all.sh
```

The terminal workflow requires the normal `NextcloudDev` build to have populated `build/macos-clang-<architecture>` with KDE Craft's CMake, Ninja, Qt, and framework dependencies.

## Output

`manifest.tsv` is the authoritative source for every capture job, output filename, production QML URL, SVG resource, and excluded legacy filename. Both the compiled capture code and shell verification derive their lists from it.

After both phases and both verification passes succeed, the 24 deliverables are available at:

```text
~/Downloads/Nextcloud UI Screenshots
```

The twelve PNG files are:

```text
dialog_activity.png
dialog_user_status.png
dialog_assistant.png
wizard_server.png
wizard_browser_auth.png
wizard_sync_options.png
settings_user.png
settings_general.png
settings_advanced.png
settings_info.png
settings_network.png
settings_ignored_files.png
```

The twelve SVG files are:

```text
icon.svg
icon-black.svg
icon-syncing.svg
icon-syncing-black.svg
icon-paused.svg
icon-paused-black.svg
icon-offline.svg
icon-offline-black.svg
icon-information.svg
icon-information-black.svg
icon-error.svg
icon-error-black.svg
```

Tray-menu capture remains manual. The workflow intentionally does not produce `traymenu.png`, `main_dialog.png`, `activities.png`, `wizard_basic_auth.png`, `activity_dialog.png`, `user_status_dialog.png`, `assistant_dialog.png`, `ignored_files_editor.png`, or any `*-white.svg` file.

The export preserves unrelated files already present in the destination. It removes and replaces only fixed, allowlisted screenshot filenames, copies each deliverable through a temporary sibling, and never recursively replaces the directory.

## Staging, run IDs, and failures

The sandbox-visible staging area is hosted at:

```text
~/Library/Containers/com.nextcloud.desktopclient/Data/tmp/nextcloud-ui-screenshots
```

Each run creates a new UUID in `.run-id`. The QML phase writes `.qml-complete` only after its six PNG and twelve SVG files are complete. The native phase receives that UUID and writes `.native-complete` only after its six PNG files are complete. All three marker contents must match before export. Markers are never copied to Downloads.

The preparation script rejects unsafe or symbolic-link paths and removes only known stale files. The native/export script refuses to start native capture when the QML markers are missing, empty, malformed, or mismatched. It propagates the standalone screenshot executable's exit status, verifies every required file is regular and nonempty, rejects excluded or white-icon files, and prints the final Downloads path as its last success line.

On failure, read the first `Nextcloud UI Screenshots:` or `Nextcloud UI Screenshots verification failed:` message in the Xcode console. A missing completion marker means that phase did not finish successfully; do not reuse files from that staging run.
