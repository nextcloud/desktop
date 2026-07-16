<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-0007: Switching the app-level File Provider mode

- **Platform:** macOS
- **Related Issues:** None yet

## Description

The File Provider integration and classic sync folders (FinderSync extension) cannot run at the same time on macOS: as soon as any file provider domain exists, the system deactivates the FinderSync extension. The client therefore offers one app-level File Provider switch in the General settings. Enabling it discards all classic sync folder connections (local files are kept); disabling it removes all file provider domains without recreating classic folders. Both directions require explicit confirmation.

## Preconditions

- A client build with the file provider module (`Nextcloud-vfs-…`) on macOS 14 or later.
- At least one configured account with one or more classic sync folders containing synced files.

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open Settings → General | A "Use macOS File Provider Integration" switch is shown with an explanatory text; it is off |
| 2 | Open the account's settings page | The "Classic sync" panel with the folder list is shown; no File Provider panel exists |
| 3 | Turn the File Provider switch on | A confirmation dialog appears, listing the affected accounts and their classic sync folder connections, stating that files stay on disk |
| 4 | Click "Cancel" | Nothing changes: switch stays off, classic folders keep syncing |
| 5 | Turn the switch on again and click "Enable File Provider" | The switch is disabled while setting up; afterwards every account appears in Finder under "Locations", all classic sync folder connections are removed, the local folders and their files remain on disk, FinderSync badges disappear, and the "Classic sync" panel is hidden in every account's settings |
| 6 | Turn the switch off | A confirmation dialog explains that files leave Finder "Locations", unsynced items are preserved, and classic sync folders are not recreated automatically |
| 7 | Click "Disable File Provider" | All file provider domains are removed; no classic folders are created; the "Classic sync" panel (with the "Add Folder Sync Connection" button) reappears in the account settings |
| 8 | Add a new folder sync connection and restart the client | The classic folder syncs again and FinderSync badges work |

## Expected Results

- Classic sync folders and File Provider are never active at the same time.
- No local files are deleted by either switch direction.
- A deployment updating with both configured (or a client killed mid-switch) shows a "Choose how to sync your files" dialog at startup offering "Keep File Provider", "Keep classic sync folders" and "Decide later"; "Decide later" keeps both working, shows a warning banner in the affected account settings, and asks again on the next launch.
- New accounts added while the mode is on automatically get a file provider domain; the account wizard skips the sync options step. With the mode off, the wizard offers classic folder setup only.
