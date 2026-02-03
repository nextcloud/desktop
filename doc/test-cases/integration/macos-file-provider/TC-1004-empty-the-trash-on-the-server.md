<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-1004: Empty the Trash on the Server

- **Platform:** macOS (File Provider)
- **Related Issues:** None yet

## Description

When emptying the trash in the Nextcloud web user interface, a previously and locally deleted file should also vanish from the local trash.

## Preconditions

- The steps of [TC-1000: Delete a File](tc-1000-delete-a-file.md)

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open the trash in the Nextcloud web user interface | See the file "Nextcloud intro.mp4" to delete |
| 2 | Empty the deleted files in the Nextcloud web user interface | All items disappear from the user interface |

## Expected Results

- The item does not appear in its original location anymore
- The item does not appear in the macOS trash
- The item no longer appears in the Nextcloud web user interface listing the trash content
