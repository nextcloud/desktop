# start.sh — Local Debug Builds

> **This script is for local development only.** While it technically contains signing and installer packaging code, those paths are not maintained for release use. For production signing, notarization, and installer creation use [mac_craft.sh](#mac_craftsh) instead.

`start.sh` compiles the client from source using CMake and Ninja, deploys it into a product directory via `mac-crafter`, and immediately launches it for testing — all in one step. It is the primary entry point for a developer working locally on the IONOS HiDrive Next macOS client.

---

## Usage

```bash
./start.sh -a <ARCHITECTURE> -b <BUILD_DIR> [options]
```

`-a` and `-b` are always required.

### Options

| Flag | Argument | Description |
|------|----------|-------------|
| `-a` | `<ARCHITECTURE>` | Target CPU architecture — `arm64` or `x86_64` (required) |
| `-b` | `<BUILD_DIR>` | Build output directory (required) |
| `-s` | `<CODE_SIGN_IDENTITY>` | Local signing identity — use your personal **Apple Development** cert. If omitted, signing is skipped |
| `-c` | | Clean rebuild — wipe the build directory before building |
| `-f` | | Build the FileProvider extension |
| `-o` | | Build as an OSX bundle (`.app`) instead of a standalone binary |
| `-r` | | Launch the app automatically after the build completes |
| `-u` | | Download and integrate Sparkle (only meaningful on clean rebuilds) |
| `-m` | | Skip `macdeployqt` |

### Recommended local build workflow

This is the standard way to build the client locally.

**First build — always do a clean build:**
```bash
./start.sh -b ~/repos/build/desktop -a "x86_64" -f -m -o -r -c
```

**All subsequent builds — omit `-c`:**
```bash
./start.sh -b ~/repos/build/desktop -a "x86_64" -f -m -o -r
```

Skipping `-c` reuses the existing build directory, so only changed files are recompiled — this is significantly faster.

### The `-m` flag and starting the app

`-m` skips `macdeployqt`, which copies all Qt frameworks and plugins into the `.app` bundle. This makes the build much faster, but has an important consequence:

| | With `-m` (recommended for dev) | Without `-m` |
|---|---|---|
| Build time | Fast | Very long |
| Start from Finder | **Does not work** — Qt dependencies are missing inside the bundle | Works |
| Start via `-r` | Works — `launch_app` sets the required library paths | Works |

**When using `-m`, the easiest way to start the app is `-r`**, which sets all required Craft library paths before launching.

Alternatively, you can set the paths manually in your terminal and launch from there:

```bash
export CRAFT="$HOME/Craft64"
export DYLD_LIBRARY_PATH="$CRAFT/lib"
export DYLD_FRAMEWORK_PATH="$CRAFT/lib"
export QT_PLUGIN_PATH="$CRAFT/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$CRAFT/plugins/platforms"
export QML2_IMPORT_PATH="$CRAFT/qml"
"$HOME/repos/build/desktop/bin/IONOS HiDrive Next.app/Contents/MacOS/IONOS HiDrive Next"
```

## What it does

1. Kills any running instance of the client
2. Runs `cmake` to configure the build (always reconfigures)
3. Runs `ninja install` to build and deploy into `<BUILD_DIR>/product`
4. If `-s` is omitted — opens the product directory in Finder; launches the app if `-r` is set
5. If `-s` is provided — signs frameworks, plugins, and the `.app` bundle with the given identity, then verifies the TeamIdentifier

## What it does NOT do (use mac_craft.sh instead)

The script contains `-i` (installer packaging) and notarization code, but these are **not suitable for release builds**:

* The installer call (`create_mac.sh`) has the IONOS team ID hardcoded
* The notarization step also uses a hardcoded IONOS keychain profile
* No white-label / STRATO support

For anything that produces a deliverable — signing a release build, building an installer, notarizing, or creating a Sparkle archive — use **mac_craft.sh**.

## Requirements

* macOS with Xcode tools installed
* Craft64 installed at `~/Craft64`
* `cmake`, `ninja`, `ccache`
* Optionally: a personal **Apple Development** certificate for local signing

---

# mac_craft.sh

This script automates the build and signing process for the **IONOS HiDrive Next** and **STRATO HiDrive Next** macOS client installers. It takes an existing `.pkg` package, optionally patches team identifiers, resigns the app, reassembles the installer, notarizes it with Apple, and finally creates a **Sparkle update package** for distribution.

---

## Features

* **Expand** a given `.pkg` file into a working directory
* **Patch** identifiers if required (team patching mode)
* **Resign** the app with the correct `Developer ID` certificates
* **Reassemble** the installer package with updated files
* **Notarize and staple** the final package with Apple's notarization service
* **Build Sparkle update** archives for app updates
* **White-label support** for STRATO HiDrive Next via `-w`

---

## Usage

```bash
./mac_craft.sh [-b <REL_BASE_DIR>] [-p <REL_PATH_TO_PKG>] [-s <TEAM_IDENTIFIER>] [-n <NC_TEAM_IDENTIFIER>] [-k <SPARKLE_KEY>] [-q <KEYCHAIN_PROFILE>] [-c] [-i] [-v] [-t] [-u] [-w]
```

### Options

| Flag | Argument | Description |
|------|----------|-------------|
| `-b` | `<REL_BASE_DIR>` | Base directory for build and output |
| `-p` | `<REL_PATH_TO_PKG>` | Path to the original `.pkg` installer |
| `-s` | `<TEAM_IDENTIFIER>` | Apple Developer Team ID of the signing entity (required) |
| `-n` | `<NC_TEAM_IDENTIFIER>` | Original Nextcloud Team ID (required when using `-t`) |
| `-k` | `<SPARKLE_KEY>` | Sparkle signing key (**currently unused**) |
| `-q` | `<KEYCHAIN_PROFILE>` | Override the notarytool keychain profile name (optional) |
| `-c` | | Clean rebuild — delete the old extracted directory before expanding |
| `-i` | | Enable installer packaging (script exits after signing if omitted) |
| `-v` | | Verbose mode — print debug output (`set -xe`) |
| `-t` | | Enable team patching — replaces `NC_TEAM_IDENTIFIER` with the signing Team ID in plists and binaries |
| `-u` | | Build and sign a Sparkle updater archive |
| `-w` | | White-label mode: build **STRATO HiDrive Next** instead of IONOS HiDrive Next |

---

## Brand defaults

The following values are set automatically based on whether `-w` is passed:

|  | IONOS (default) | STRATO (`-w`) |
|--|-----------------|----------------|
| Product name | `IONOS HiDrive Next` | `STRATO HiDrive Next` |
| Code sign identity | `IONOS SE (<TEAM_ID>)` | `STRATO AG (<TEAM_ID>)` |
| Keychain profile | `IONOS SE HiDrive Next` | `STRATO AG HiDrive Next` |

Use `-q` to override the keychain profile name when the stored credentials use a different name.

### Setting up notarization credentials

Each brand requires its own notarytool keychain profile to be set up once per machine by a member of the corresponding Apple Developer team:

```bash
# IONOS
xcrun notarytool store-credentials "IONOS SE HiDrive Next" \
  --apple-id "developer@ionos.com" \
  --team-id "<IONOS_TEAM_ID>" \
  --password "xxxx-xxxx-xxxx-xxxx"

# STRATO
xcrun notarytool store-credentials "STRATO AG HiDrive Next" \
  --apple-id "developer@strato.com" \
  --team-id "<STRATO_TEAM_ID>" \
  --password "xxxx-xxxx-xxxx-xxxx"
```

The password is an **app-specific password** generated at [appleid.apple.com](https://appleid.apple.com) — not the Apple ID login password. The Apple ID used must be a member of the respective Apple Developer team.

---

## Workflow

1. **Expand Original Package**

   * The `.pkg` is expanded into a working directory (`pkgutil --expand-full`).
   * If `-c` is set, any previous working directory is removed first.

2. **Patch Identifiers (Optional)**

   * If `-t` is used, the script searches `.plist` and binary files for the old Nextcloud team identifier.
   * It replaces it with the signing `TEAM_IDENTIFIER` passed via `-s`.
   * Both IDs must have the same character length to ensure safe binary patching.

3. **Resign Application**

   * The client `.app` is signed using the `mac-crafter` tool.
   * Codesign identity is brand-dependent (see [Brand defaults](#brand-defaults) above).
   * Verifies that the signed app's `TeamIdentifier` matches the expected one.

4. **Reassemble Installer**

   * Recreates the package payload (`mkbom`, `cpio`, `pkgutil --flatten`).
   * Signs the inner package and final installer using the brand's `Developer ID Installer` certificate.
   * Uses `productbuild` and `productsign` to generate the final signed package.

5. **Notarization & Stapling**

   * Submits the package to Apple's Notary Service (`xcrun notarytool`).
   * Uses the brand's keychain profile (overridable via `-q`).
   * Waits for the result and validates acceptance.
   * Applies a **staple** to the installer (`xcrun stapler staple`).

6. **Sparkle Update Build (Optional)**

   * Downloads Sparkle if not already present.
   * Archives the signed package as `.tbz`.
   * Signs the archive using Sparkle's `sign_update` tool.

---

## Examples

**IONOS build:**
```bash
./mac_craft.sh \
  -b /Users/developer/build \
  -p ./IONOS.pkg \
  -s ABC123XYZ \
  -n OLDTEAMID \
  -i -c -t -u -v
```

**STRATO build:**
```bash
./mac_craft.sh \
  -b /Users/developer/build \
  -p ./STRATO.pkg \
  -s DEF456UVW \
  -n OLDTEAMID \
  -i -c -t -u -v -w
```

**With custom keychain profile:**
```bash
./mac_craft.sh \
  -b /Users/developer/build \
  -p ./STRATO.pkg \
  -s DEF456UVW \
  -i -w -q "My Custom Profile"
```

---

## Requirements

* macOS with Xcode tools installed
* Apple Developer account with:
  * **Developer ID Application** certificate for the signing entity
  * **Developer ID Installer** certificate for the signing entity
* Notarytool keychain profile stored on the build machine (see [Setting up notarization credentials](#setting-up-notarization-credentials))
* `mac-crafter` tool (Swift package, provided by Nextcloud)
* System tools: `pkgutil`, `productbuild`, `productsign`, `notarytool`, `stapler`, `xcrun`, `grep`, `sed`, `mkbom`, `cpio`, `gzip`
* `wget`, `tar`, `perl` for Sparkle integration

---

## Output

* Final notarized installer:
  ```
  <BASE_DIR>/<PKG_FILENAME>.resigned.pkg
  ```
* Sparkle update archive (if `-u` enabled):
  ```
  <BASE_DIR>/<PKG_FILENAME>.resigned.pkg.tbz
  ```
