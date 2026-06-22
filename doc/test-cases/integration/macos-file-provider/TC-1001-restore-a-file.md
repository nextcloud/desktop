<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-1001: Restore a File

- **Platform:** All
- **Related Issues:** None yet

## Description

A previously deleted file can be restored to its original location.

## Preconditions

- The steps of [TC-1000: Delete a File](TC-1000-delete-a-file.md)

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open the trash from the Dock | "Nextcloud intro.mp4" now is located there as the only item |
| 2 | Open the context menu on "Nextcloud intro.mp4" | A menu item to restore it is listed |
| 3 | Select the menu item to restore it | The file disappears from the Finder window which presents the trash content |

## Expected Results

- The item restored from the trash does not appear in the trash anymore
- The item restored from the trash does appear in the original folder in Finder again
- The item restored from the trash does appear in the original folder in the Nextcloud web user interface again
