<!--
  - SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
# mac-crafter

mac-crafter is a tool to easily build a fully functional Nextcloud Desktop Client for macOS.
It automates cloning, configuring, crafting, codesigning, packaging, and even DMG creation of the client.
The tool is built with Swift’s ArgumentParser and it drives the KDE Craft build system along with some Python scripts and shell commands.

## System Requirements

- macOS 11 Big Sur or newer
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

mac-crafter comes with several subcommands:

### Build

This is the default command and it handles:
- Configuring and/or cloning KDE Craft (using the CraftMaster repository)
- Adding the Nextcloud Desktop Client blueprints
- Crafting KDE Craft projects and installing dependencies
- Building the client with options for a full rebuild, offline mode, and more

**Usage Example:**

```
swift run mac-crafter [options]
```

**Common Options:**

- **Repository and Build Paths:**
  - `--repo-root-dir`: Path to the Nextcloud Desktop Client git repository (default is `../../../` relative to the current directory).
  - `--build-path`: Directory where build files are written.
  - `--product-path`: Directory where the final product (app bundle) will be placed.

- **Build Settings:**
  - `--arch`: Architecture to build for (e.g. `arm64`, `x86_64`).
  - `--build-type`: Build type (e.g. `Release`, `RelWithDebInfo`, `Debug`).
  - `--craft-blueprint-name`: Blueprint name for Nextcloud Desktop Client (default is `"nextcloud-client"`).
  - `--full-rebuild`: Forces a full rebuild by wiping existing build artifacts.
  - `--offline`: Run the build offline (do not update craft).

- **Code Signing & Notarisation:**
  - `--code-sign-identity (-c)`: Code signing identity for the client and libraries.
  - `--apple-id`, `--apple-password`, `--apple-team-id`: Credentials for notarisation.
  - `--package-signing-id`: Identifier used for package signing.

- **Advanced Options:**
  - `--disable-autoupdater`: Build without the Sparkle auto-updater.
  - `--build-tests`: Optionally build the test suite.
  - `--build-file-provider-module`: Build the File Provider Module.
  - `--dev`: Build in developer mode which, for example, appends "Dev" to the app name and sets a dev flag in the craft options.
  - `--override-server-url` and `--force-override-server-url`: Override server URL settings for the client.

The build process automatically ensures necessary tools (like git, inkscape, python3) are installed—invoking installation commands on missing dependencies.

### Codesign

Use this subcommand to codesign an existing Nextcloud Desktop Client app bundle.

**Usage Example:**

```
swift run mac-crafter codesign -c "Apple Development: <certificate common name>" <path-to-app-bundle>
```

- **Options:**
  - `appBundlePath`: Path to the app bundle.
  - `--code-sign-identity (-c)`: Code signing identity to use.

### Package

This command is used to package the client after building. It prepares the app bundle and can also perform package signing and notarisation.

**Usage Example:**

```
swift run mac-crafter package [options]
```

- **Options:**
  - `--arch`: Target architecture.
  - `--build-path`, `--product-path`: Build and product directories.
  - `--craft-blueprint-name`: Blueprint name.
  - `--app-name`: The branded name of the application.
  - Various notarisation options (`--apple-id`, `--apple-password`, `--apple-team-id`).
  - Signing options such as `--package-signing-id` and `--sparkle-package-sign-key`.

### CreateDMG

This subcommand creates a DMG (disk image) for the client app bundle.

**Usage Example:**

```
swift run mac-crafter createDMG <path-to-app-bundle> [options]
```

- **Options:**
  - `appBundlePath`: The app bundle’s path.
  - `--product-path`: Where the final DMG and product will be placed.
  - `--build-path`: Directory for temporary build files.
  - `--app-name`: Application's name.
  - Notarisation and signing options similar to the Package command.

## How It Works

1. **Tooling Configuration:**
   The build command checks for necessary tools (like `codesign`, `git`, `brew`, `inkscape`, and `python3`) and auto-installs missing dependencies if needed.  
2. **KDE Craft Configuration:**
   - If KDE Craft isn’t already cloned or if a reconfiguration is triggered, the tool clones the CraftMaster repository and configures it using a provided INI file.
   - Next, it adds the Nextcloud Desktop Client blueprints, then crafts KDE Craft and installs the required dependencies.
3. **Craft Options Setup:**
   The build process assembles a set of options including source directory, architecture, build tests, auto-updater settings, and more.  
4. **Building, Codesigning, and Packaging:**
   The tool then builds the client, optionally performs a full rebuild, and if a codesign identity is provided, signs the final app bundle. Finally, it copies the finished app bundle to the product directory.
5. **Optional DMG Creation:**
   Use the CreateDMG subcommand to bundle the built client into a DMG for distribution.

## Quick Start

For a basic build and codesigning:
```
swift run mac-crafter -c "Apple Development: MyCertificate"
```

For a full rebuild on a specific architecture:
```
swift run mac-crafter --arch arm64 --full-rebuild -c "Apple Development: MyCertificate"
```

To package the app:
```
swift run mac-crafter package -c "Apple Development: MyCertificate" --arch arm64
```

To create a DMG:
```
swift run mac-crafter createDMG /path/to/Nextcloud.app --app-name Nextcloud
```

For more details on all available options, run:
```
swift run mac-crafter --help
```

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

## License

Distributed under the terms of the GPL-2.0-or-later license.
