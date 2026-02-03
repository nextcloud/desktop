<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-1000: Delete a File

- **Platform:** All
- **Related Issues:** None yet

## Description

A file moved to the trash locally should also be moved likewise on the server while appearing in the local and remote trash.

## Preconditions

- Have a fresh Nextcloud account with the default demo content
- Have an account and file provider domain set up and ready on macOS
- macOS trash is empty

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open Finder | The sidebar lists a location called "Nextcloud" |
| 2 | Select the "Nextcloud" location | The items in the Nextcloud user's root folder appear in Finder |
| 3 | Open the context menu on "Nextcloud intro.mp4" | A menu item to move it to the trash is listed |
| 4 | Select the menu item to move it to the trash | The file disappears from the Finder window |
| 5 | Open the trash from the Dock | "Nextcloud intro.mp4" now is located there as the only item |

## Expected Results

- The item moved to the trash does not appear in its original location anymore
- The item moved to the trash does appear in the macOS trash
- The item moved to the trash does appear in the Nextcloud trash web user interface
