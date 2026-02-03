<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-1002: Empty the Trash

- **Platform:** macOS (File Provider)
- **Related Issues:** None yet

## Description

When emptying the trash of macOS, an item should be removed permanently on the local device as on the Nextcloud server.

## Preconditions

- The steps of [TC-1000: Delete a File](tc-1000-delete-a-file.md)

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open the trash from the Dock | "Nextcloud intro.mp4" now is located there as the only item |
| 2 | Click the button to empty the trash in the Finder header bar | The trash is emptied and left without content |

## Expected Results

- The deleted item does not appear in its original folder in Finder anymore
- The deleted item does not appear in the trash in Finder anymore
- The deleted item does not appear in its original folder in the Nextcloud web user interface anymore
- The deleted item does not appear in the trash in the Nextcloud web user interface anymore
