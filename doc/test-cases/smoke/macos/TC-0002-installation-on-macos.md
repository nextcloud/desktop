<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-0002: Installation on macOS

- **Platform:** macOS
- **Related Issues:** None yet

## Description

Check whether the installation of the client for macOS completes successfully.

## Preconditions

- Have an installer package. Usually named like `Nextcloud-0.0.0.pkg`.

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open the installer package | The macOS installation app appears with a welcome message |
| 2 | Click "Continue" button | The app informs about the disk space expected to be occupied |
| 3 | Click "Install" button | macOS asks the user to authenticate before installing |
| 4 | Authenticate | Installation app continues with progress reporting about installation progress until finished |
| 5 | Click "Finish" | Installation app terminates |

## Expected Results

- You have a "Nextcloud.app" in your "/Applications" folder.
