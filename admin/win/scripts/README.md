<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Windows Build Scripts

This directory contains Windows batch scripts for building, packaging, signing, and uploading
the Nextcloud Desktop Client on Windows. They were originally maintained in the separate
[nextcloud/client-building](https://github.com/nextcloud/client-building) repository and have
been moved here so that build tooling lives alongside the source it builds.

> **Note:** NSIS EXE installer creation is **not** supported by these scripts.
> The official Windows installer is the WiX-based MSI (see `admin/win/msi/`).

---

## Prerequisites

- Windows 10 or later (64-bit)
- [KDE Craft](https://community.kde.org/Get_Involved/development/Windows) with all
  `nextcloud-client` dependencies installed
- Visual Studio 2022 (Community or higher) with C++ workload
- [WiX Toolset v3](https://wixtoolset.org/releases/) installed and `WIX` environment
  variable pointing to its root (e.g. `C:\Program Files (x86)\WiX Toolset v3.14`)
- [Git for Windows](https://git-scm.com/download/win) (provides Git Bash and `mkdir.exe`)
- [png2ico](https://www.winterdrache.de/freeware/png2ico/) placed at
  `C:\Nextcloud\tools\png2ico.exe` (or override `Png2Ico_EXECUTABLE`)

For code signing (optional):

- A valid code-signing certificate in PFX format
- Set `CERTIFICATE_FILENAME`, `CERTIFICATE_CSP`, `CERTIFICATE_KEY_CONTAINER_NAME`, and
  `CERTIFICATE_PASSWORD` in your environment before running

---

## Quick Start

Open a **Git Bash** shell in the repository root and run:

```bat
cd admin/win/scripts
./build.bat
```

This will, in order:

1. Build the desktop client (`build-desktop.bat`)
2. Collect all runtime libraries and resources (`build-installer-collect.bat`)
3. Build the MSI installer (`build-installer-msi.bat`)

To run without signing or uploading:

```bat
USE_CODE_SIGNING=0 UPLOAD_BUILD=0 ./build.bat Release
```

To do a dry run (validates all required variables without building):

```bat
TEST_RUN=1 ./build.bat
```

---

## Environment Variables

All variables have sensible defaults defined in `defaults.inc.bat`. You may override any of
them in your shell environment before calling any script.

| Variable | Default | Description |
|---|---|---|
| `DESKTOP_REPO_PATH` | derived from script location | Absolute path to the repository root (auto-detected) |
| `PROJECT_PATH` | `c:/Nextcloud` | Workspace root for build artifacts (`install/`, `collect/`, `daily/`) |
| `BUILD_TYPE` | `RelWithDebInfo` | CMake build type (`Release`, `Debug`, `RelWithDebInfo`) |
| `BUILD_TARGETS` | `Win64` | Comma-separated architectures to build (`Win64`, `Win32`, or both) |
| `CRAFT_PATH` | auto (Craft default) | Path to the KDE Craft root for the target architecture |
| `VS_VERSION` | `2022` | Visual Studio version (`2017`, `2019`, `2022`) |
| `VCINSTALLDIR` | auto from `VS_VERSION` | Path to the Visual C++ installation directory |
| `WIN_GIT_PATH` | `C:\Program Files\Git` | Git for Windows root (provides `mkdir.exe`) |
| `Png2Ico_EXECUTABLE` | `c:/Nextcloud/tools/png2ico.exe` | Path to the png2ico converter |
| `WIX` | `C:/Program Files (x86)/WiX Toolset v3.14` | WiX Toolset installation directory |
| `WIX_SDK_PATH` | `%WIX%/SDK/VS2017` | WiX SDK path used by the NCMsiHelper CMake target |
| `BUILD_INSTALLER_MSI` | `1` | Set to `0` to skip MSI creation |
| `USE_CODE_SIGNING` | `1` | Set to `0` to skip all signing steps |
| `UPLOAD_BUILD` | `1` | Set to `0` to skip the SCP upload step |
| `INSTALLER_OUTPUT_PATH` | `%PROJECT_PATH%/daily/` | Directory where the finished MSI is placed |
| `EXTRA_DEPLOY_PATH` | `%PROJECT_PATH%/deploy-extra/%BUILD_TYPE%/%BUILD_ARCH%` | Optional directory of extra DLLs to bundle |
| `PULL_DESKTOP` | `0` | Set to `1` to clone a fresh copy of the repository before building |
| `CHECKOUT_DESKTOP` | `0` | Set to `1` together with `PULL_DESKTOP=1` to choose a specific tag |
| `TAG_DESKTOP` | `master` | Branch or tag to clone when `CHECKOUT_DESKTOP=1` |
| `CMAKE_EXTRA_FLAGS_DESKTOP` | *(empty)* | Extra CMake flags forwarded to the desktop build |
| `BUILD_UPDATER` | `OFF` | Set to `ON` to build the auto-updater |

For code signing, additionally set:

| Variable | Description |
|---|---|
| `CERTIFICATE_FILENAME` | Path to the PFX certificate file |
| `CERTIFICATE_CSP` | Cryptographic Service Provider name |
| `CERTIFICATE_KEY_CONTAINER_NAME` | Key container name inside the CSP |
| `CERTIFICATE_PASSWORD` | Certificate password |
| `SIGN_FILE_DIGEST_ALG` | Digest algorithm (default: `sha256`) |
| `SIGN_TIMESTAMP_URL` | Timestamp server URL (default: `http://timestamp.digicert.com`) |
| `SIGN_TIMESTAMP_DIGEST_ALG` | Timestamp digest algorithm (default: `sha256`) |
| `APPLICATION_VENDOR` | Vendor string used in the signing description (default: `Nextcloud GmbH`) |

For uploading (`UPLOAD_BUILD=1`), additionally set:

| Variable | Description |
|---|---|
| `SFTP_SERVER` | Hostname of the SSH/SCP target server |
| `SFTP_USER` | SSH username |
| `SFTP_PATH` | Remote path on the server |

---

## Script Reference

### Top-level entry points

| Script | Purpose |
|---|---|
| `build.bat` | **Main entry point.** Runs build → collect → MSI in sequence. |
| `task-build-log.bat` | Same as `build.bat` but writes output to a timestamped log file under `%PROJECT_PATH%/logs/`. Suitable for Windows Task Scheduler. |
| `task-build-job.sh` | Thin Git Bash wrapper around `task-build-log.bat` for use in Task Scheduler jobs configured with Git Bash. |

### Per-stage loop wrappers

These iterate over `BUILD_TARGETS` and call the corresponding `single-build-*.bat` helper for
each target architecture.

| Script | Purpose |
|---|---|
| `build-desktop.bat` | Build the desktop client for all configured architectures. |
| `build-installer-collect.bat` | Collect all runtime files for each architecture. |
| `build-installer-msi.bat` | Build and (optionally) sign and upload the MSI for each architecture. |

### Per-architecture implementation scripts

Call these directly when you want to build for a single architecture:

```bat
single-build-desktop.bat <BUILD_TYPE> <BUILD_ARCH>
single-build-installer-collect.bat <BUILD_TYPE> <BUILD_ARCH>
single-build-installer-msi.bat <BUILD_TYPE> <BUILD_ARCH>
```

Example (Release, 64-bit only):

```bat
single-build-desktop.bat Release Win64
single-build-installer-collect.bat Release Win64
single-build-installer-msi.bat Release Win64
```

### Utility scripts

| Script | Purpose |
|---|---|
| `sign.bat <file>` | Sign a single binary with `signtool`. Reads certificate settings from environment. |
| `upload.bat <file>` | Upload a file to the configured SCP server. |

### Include files

| Script | Purpose |
|---|---|
| `defaults.inc.bat` | Sets all default environment variables. Sourced by every script. |
| `common.inc.bat` | Sets architecture-specific variables (`BUILD_ARCH`, `CRAFT_PATH`, `QT_*`). |
| `datetime.inc.bat` | Sets `_date` and `_time` in a locale-independent format. |
| `datetime.inc.callee.sh` | Git Bash helper called by `datetime.inc.bat` to obtain the current date. |

---

## Artifact Layout

After a successful build the following directories are created under `PROJECT_PATH`:

```
install/<BUILD_TYPE>/<BUILD_ARCH>/
    bin/            — compiled binaries installed by cmake --target install
    msi/            — MSI build helper DLL and WiX scripts (from CMake install)
    ...

collect/<BUILD_TYPE>/<BUILD_ARCH>/
    *.dll           — all runtime DLLs needed by the client
    *.exe           — client executables
    ...

daily/
    Nextcloud-<version>-<arch>.msi   — final signed MSI installer
```

---

## Relationship to `admin/win/msi/`

The scripts in this directory are **orchestration wrappers**. The actual WiX source files and
CMake targets that compile the MSI helper DLL (`NCMsiHelper`) and generate `make-msi.bat` live
in `admin/win/msi/` and `admin/win/tools/`. The final MSI build step in
`single-build-installer-msi.bat` calls `make-msi.bat` (placed by CMake install into
`install/<type>/<arch>/msi/`) with the collect directory as input.

---

## What is NOT included

- **NSIS EXE installer creation** — The legacy NSIS installer (`nextcloud.nsi`) and the
  wrapper script `build-installer-exe.bat` are not included here. The WiX MSI is the current
  official Windows installer format.
- **Dependency bootstrapping** (`init.bat`, `build-qtkeychain.bat`, `build-zlib.bat`) — These
  were part of the original standalone `client-building` setup and are now obsolete. All
  build-time dependencies are provided by KDE Craft.
