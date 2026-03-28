<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-1003: Delete a File on the Server

- **Platform:** All
- **Related Issues:** None yet

## Description

When moving a file into the trash in the Nextcloud web user interface, it should vanish from the file provider domain and not show up in the local trash.

## Preconditions

- Have a file in the Nextcloud account. For clarity, it will be referred to as "Test.md" from here on but can be named arbitrarily
- Have the client and account set up on a Mac with file provider domain enabled
- See the file in question in Finder in the file provider domain location

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open the Nextcloud web user interface | See the file "Test.md" to delete |
| 2 | Delete the file in the web user interface | The file "Test.md" appears in the trash in the web user interface |

## Expected Results

- The deleted item does not appear in its original folder in Finder anymore
- The deleted item does also not appear in the trash in Finder
- The deleted item does not appear in its original folder in the Nextcloud web user interface anymore
- The deleted item does appear in the trash in the Nextcloud web user interface
