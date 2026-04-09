# mac_craft.sh

This script automates the build and signing process for the **IONOS HiDrive Next** macOS client installer. It takes an existing `.pkg` package, optionally patches team identifiers, resigns the app, reassembles the installer, notarizes it with Apple, and finally creates a **Sparkle update package** for distribution.

---

## Features

* **Expand** a given `.pkg` file into a working directory
* **Patch** identifiers if required (team patching mode)
* **Resign** the app with the correct `Developer ID` certificates
* **Reassemble** the installer package with updated files
* **Notarize and staple** the final package with Apple’s notarization service
* **Build Sparkle update** archives for app updates

---

## Usage

```bash
./mac_craft.sh [-b <REL_BASE_DIR>] [-p <REL_PATH_TO_PKG>] [-s <IONOS_TEAM_IDENTIFIER>] [-n <NC_TEAM_IDENTIFIER>] [-k <SPARKLE_KEY>] [-c] [-i] [-v] [-t] [-u]
```

### Options

* `-b <REL_BASE_DIR>` : Base directory for build and output
* `-p <REL_PATH_TO_PKG>` : Path to the original `.pkg` installer
* `-s <IONOS_TEAM_IDENTIFIER>` : IONOS Team Identifier (required for signing)
* `-n <NC_TEAM_IDENTIFIER>` : Old Team Identifier (used for patching if needed)
* `-k <SPARKLE_KEY>` : Sparkle signing key (**currently unused**)
* `-c` : Clean rebuild (delete old extracted directory before expanding)
* `-i` : Enable installer packaging (otherwise exits after signing)
* `-v` : Verbose mode (print debug output)
* `-t` : Enable team patching mode (replaces `NC_TEAM_IDENTIFIER` with `IONOS_TEAM_IDENTIFIER`)
* `-u` : Build Sparkle updater package

---

## Workflow

1. **Expand Original Package**

   * The `.pkg` is expanded into a working directory (`pkgutil --expand-full`).
   * If `-c` is set, any previous working directory is removed first.

2. **Patch Identifiers (Optional)**

   * If `-t` is used, the script searches `.plist` and binary files for the old team identifier.
   * It replaces it with the new `IONOS_TEAM_IDENTIFIER`.
   * Both IDs must have the same character length to ensure safe binary patching.

3. **Resign Application**

   * The client `.app` is signed using the `mac-crafter` tool.
   * Codesign identity: `Developer ID Application: IONOS SE (TEAM_ID)`
   * Verifies that the signed app’s TeamIdentifier matches the expected one.

4. **Reassemble Installer**

   * Recreates the package payload (`mkbom`, `cpio`, `pkgutil --flatten`).
   * Signs the installer with:
     `Developer ID Installer: IONOS SE (TEAM_ID)`
   * Uses `productbuild` and `productsign` to generate the final signed package.

5. **Notarization & Stapling**

   * Submits the package to Apple’s Notary Service (`xcrun notarytool`).
   * Waits for the result, validates acceptance.
   * Applies a **staple** to the installer (`xcrun stapler staple`).

6. **Sparkle Update Build (Optional)**

   * Downloads Sparkle if not available.
   * Archives the signed package as `.tbz`.
   * Signs the archive using Sparkle’s `sign_update` tool.

---

## Example

```bash
./mac_craft.sh \
  -b /Users/developer/build \
  -p ./IONOS.pkg \
  -s ABC123XYZ \
  -n OLDTEAMID \
  -i -c -t -u -v
```

This command will:

* Clean and rebuild the working directory
* Expand `IONOS.pkg`
* Patch from `OLDTEAMID` → `ABC123XYZ`
* Resign the app and installer
* Notarize and staple the package
* Build a signed Sparkle update archive
* Run in verbose mode

---

## Requirements

* macOS with Xcode tools installed
* Apple Developer account with:

  * **Developer ID Application** certificate
  * **Developer ID Installer** certificate
* `mac-crafter` tool (Swift package, provided by Nextcloud)
* `pkgutil`, `productbuild`, `productsign`, `notarytool`, `stapler`, `xcrun`, `grep`, `sed`, `mkbom`, `cpio`, `gzip`
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
