<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# TC-0006: Account Setup on macOS (File Provider)

- **Platform:** macOS
- **Related Issues:** None yet

## Description

Go through the initial account setup on a fresh installation.

## Preconditions

- The installation previously was completed as described in [TC-0005: Installation on macOS (File Provider)](./TC-0005-installation-on-macos-file-provider.md)
- Visibility of cloud storage locations in Finder sidebar is enabled in its settings.

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open "Nextcloud.app" in "/Applications" | The onboarding window appears, the menu bar extra shows the brand logo |
| 2 | Click the log in button | The form for entering the server address appears |
| 3 | Enter a valid server address and click the continue button | The client opens the login flow in the default browser, the client shows a user interface about waiting for the authorization |
| 4 | Complete the login flow in the browser | The web user interface informs about the expected authorization |
| 5 | Switch back to the client app | The client recognizes the authorization and closes its window |

## Expected Results

- The menu bar extra presents synchronization status symbols.
- Finder now has a "Nextcloud" item in the "Locations" section.
- Navigating to the "Nextcloud" location, Finder lists the items of the account's root folder on the Nextcloud server.
- When opening the settings for the newly setup account in the client settings, the file provider extension is selected as enabled because it should be enabled by default.
