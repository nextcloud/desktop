<!--
  - SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
# Mac Crafter

mac-crafter is a tool to easily build a fully functional Nextcloud Desktop Client for macOS.
It automates cloning, configuring, crafting, codesigning, packaging, and even disk image creation of the client.
The tool is built with Apple’s ArgumentParser and it drives the KDE Craft build system along with some Python scripts and shell commands.

## System Requirements

- macOS 12 Monterey or newer
- Xcode
- Python3
- Homebrew (for installing additional tools like `inkscape`, `pyenv`, and `create-dmg`)

## Installation

After cloning the Nextcloud Desktop Client repository, navigate to the `admin/osx/mac-crafter` directory and run:

```bash
swift run mac-crafter
```

This will automatically check for the required tools and install them if they are missing.
The script will also clone the KDE Craft repository if it is not already present.

## Usage

mac-crafter comes with several subcommands.
To see a full reference, run `mac-crafter --help` or `mac-crafter <subcommand> --help` for further specific information about the command.

## Additional Information

- **Python Script for Universal App Bundles:**
  To build a universal app bundle (supporting both arm64 and x86_64), use the provided `make_universal.py` script located in `admin/osx`:
  ```
  python admin/osx/make_universal.py <x86 build path> <arm64 build path> <final target path>
  ```

- **Sparkle Auto-Updater:**
  If enabled (default), the tool downloads and unpacks the Sparkle framework used for client auto-updates. This step is skipped if `--disable-autoupdater` is provided.

- **Notarisation:**
  For signing and notarisation on macOS, appropriate options for Apple ID and password can be provided.

## Development

`mac-crafter` can also be run and debugged with Xcode.
Additional preparation is necessary, though.

1. Edit the automatically generated "mac-crafter" scheme in Xcode.
2. Navigate to the "Run" action.
3. Navigate to the "Arguments" tab. 
4. Define the arguments you want to pass to the program as you would do in a terminal.
5. Define the "PATH" environment variable. You can copy and paste the output of `echo "$PATH"` from a terminal. Otherwise the executable will not inherit your shell environment paths as necessary for Homebrew to work.
6. Navigate to the "Options" tab.
7. Enable and define a custom working directory. The root of this Swift package, to be specific. 

## License

Distributed under the terms of the GPL-2.0-or-later license.
