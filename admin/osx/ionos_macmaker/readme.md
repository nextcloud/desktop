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
