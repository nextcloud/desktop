<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Windows Build Scripts

PowerShell scripts for building, packaging, signing, and uploading the
Nextcloud Desktop client on Windows.

These scripts supersede the batch scripts previously maintained in the
[nextcloud/client-building](https://github.com/nextcloud/client-building)
repository.  A [migration guide](#migrating-from-client-building) is included
at the bottom of this document.

---

## Prerequisites

| Tool | Notes |
|---|---|
| Windows 10 / Server 2019 or later | Required for built-in OpenSSH (`scp`) |
| PowerShell 5.1 or later | Ships with Windows 10; PowerShell 7 also works |
| Visual Studio 2022 (Community or higher) | C++ and Windows SDK workloads required |
| CMake ≥ 3.16 | Must be in `PATH` |
| Ninja | Used as the CMake generator |
| Qt via KDE Craft | See [desktop-client-blueprints](https://github.com/nextcloud/desktop-client-blueprints/) |
| WiX Toolset v3 | <https://wixtoolset.org/releases/>; needed for MSI packaging |
| Git for Windows | Must be in `PATH` |

---

## Directory layout

```
admin/win/
├── build/                  ← build scripts (this folder)
│   ├── common.ps1          ← shared helper functions (dot-sourced library)
│   ├── defaults.ps1        ← default environment variable values (dot-sourced)
│   ├── init.ps1            ← one-time workspace initialisation
│   ├── build.ps1           ← full build orchestrator
│   ├── build-desktop.ps1   ← CMake configure / build / install
│   ├── build-installer-collect.ps1  ← file staging + Qt deploy + signing
│   ├── build-installer-msi.ps1      ← WiX MSI creation + signing + upload
│   ├── sign.ps1            ← standalone code-signing entry point
│   ├── upload.ps1          ← standalone upload entry point
│   └── task-build-log.ps1  ← Task Scheduler logging wrapper
└── msi/
    ├── make-msi.ps1.in     ← CMake template → make-msi.ps1 (in install tree)
    └── …                   ← WXS sources and supporting files
```

---

## Quick start

```powershell
# 1. Set the workspace root (a directory on a fast local drive)
$env:PROJECT_PATH = 'C:\NextcloudBuild'

# 2. One-time: clone all repositories and create directories
.\admin\win\build\init.ps1

# 3. Full build (desktop + collect + MSI) without signing or uploading
$env:USE_CODE_SIGNING = '0'
$env:UPLOAD_BUILD     = '0'
.\admin\win\build\build.ps1 -BuildType Release

# 4. The resulting MSI is at:
#    C:\NextcloudBuild\daily\Nextcloud-<version>-x64.msi
```

Run individual stages independently when iterating on a specific step:

```powershell
.\admin\win\build\build-desktop.ps1           -BuildType Release
.\admin\win\build\build-installer-collect.ps1 -BuildType Release
.\admin\win\build\build-installer-msi.ps1     -BuildType Release
```

---

## Environment variables

All variables can be set before invoking any script.  `defaults.ps1` applies
the listed defaults only when a variable is **not already set**.

### Paths

| Variable | Default | Description |
|---|---|---|
| `PROJECT_PATH` | `c:/Nextcloud/client-building` | Build workspace root |
| `Png2Ico_EXECUTABLE` | `c:/Nextcloud/tools/png2ico.exe` | png2ico tool path |
| `WIN_GIT_PATH` | `C:\Program Files\Git` | Git for Windows installation |
| `INSTALLER_OUTPUT_PATH` | `$PROJECT_PATH/daily` | Destination for finished installers |

### Build configuration

| Variable | Default | Description |
|---|---|---|
| `BUILD_TYPE` | `RelWithDebInfo` | CMake build type |
| `BUILD_TARGETS` | `Win64` | Space- or comma-separated list of target architectures (`Win64`, `Win32`) |
| `BUILD_DATE` | today (`yyyyMMdd`) | Build date embedded in the installer |
| `VERSION_SUFFIX` | _(empty)_ | Extra version suffix (e.g. `-beta1`) |
| `TAG_DESKTOP` | `master` | Git tag or branch to build |
| `CRAFT_TAG_DESKTOP` | `master` | KDE Craft branch name (determines `CRAFT_PATH`) |
| `BUILD_UPDATER` | `OFF` | Pass `ON` to build the auto-updater |
| `BUILD_INSTALLER_MSI` | `1` | Set to `0` to skip MSI packaging |
| `CMAKE_EXTRA_FLAGS_DESKTOP` | _(empty)_ | Extra `-D` flags forwarded to CMake |
| `TEST_RUN` | `0` | Set to `1` for a dry run (prints variables, no actions) |

### Git behaviour

| Variable | Default | Description |
|---|---|---|
| `PULL_DESKTOP` | `1` | Re-clone the desktop source on each build |
| `CHECKOUT_DESKTOP` | `1` | Check out `TAG_DESKTOP` (implies a fresh clone) |
| `USE_BRANDING` | `0` | Set to `1` to disable auto-pull (for custom branding builds) |

### Visual Studio

| Variable | Default | Description |
|---|---|---|
| `VS_VERSION` | `2022` | Visual Studio version (`2017`, `2019`, `2022`) |
| `VCINSTALLDIR` | auto-detected from `VS_VERSION` | Path to the VC directory |

### Code signing

| Variable | Default | Description |
|---|---|---|
| `USE_CODE_SIGNING` | `1` | Set to `0` to disable signing |
| `APPLICATION_VENDOR` | `Nextcloud GmbH` | Vendor string in the signature |
| `CERTIFICATE_FILENAME` | _(required)_ | Path to the `.pfx` / `.p12` certificate file |
| `CERTIFICATE_CSP` | _(required)_ | Cryptographic Service Provider name |
| `CERTIFICATE_KEY_CONTAINER_NAME` | _(required)_ | Key container name |
| `CERTIFICATE_PASSWORD` | _(required)_ | Certificate password |
| `SIGN_FILE_DIGEST_ALG` | `sha256` | File digest algorithm |
| `SIGN_TIMESTAMP_URL` | `http://timestamp.digicert.com` | RFC 3161 timestamp URL |
| `SIGN_TIMESTAMP_DIGEST_ALG` | `sha256` | Timestamp digest algorithm |
| `SIGNTOOL` | _(auto-detected)_ | Full path to `signtool.exe`; detected from `PATH` when not set |

### Upload

| Variable | Default | Description |
|---|---|---|
| `UPLOAD_BUILD` | `1` | Set to `0` to skip upload |
| `UPLOAD_DELETE` | `0` | Set to `1` to delete the local MSI after a successful upload |
| `SFTP_SERVER` | _(required)_ | SFTP / SCP host |
| `SFTP_USER` | _(required)_ | Username for the SCP connection |
| `SFTP_PATH` | `/var/www/html/desktop/daily/Windows` | Remote destination path |

The upload uses `scp` with the SSH key at `%USERPROFILE%\.ssh\id_rsa`.

---

## Code signing setup

1. Obtain a code-signing certificate and export it as a `.pfx` or `.p12` file.
2. Import the certificate into the Windows Certificate Store or keep it as a
   file accessible to the build machine.
3. Set the required environment variables before building:

```powershell
$env:USE_CODE_SIGNING               = '1'
$env:CERTIFICATE_FILENAME           = 'C:\certs\nextcloud.pfx'
$env:CERTIFICATE_CSP                = 'Microsoft Software Key Storage Provider'
$env:CERTIFICATE_KEY_CONTAINER_NAME = 'NextcloudCodeSign'
$env:CERTIFICATE_PASSWORD           = 'secret'
```

To sign an individual file without running a full build:

```powershell
.\admin\win\build\sign.ps1 -FilePath 'C:\path\to\file.exe'
```

---

## Scheduled builds with Windows Task Scheduler

`task-build-log.ps1` wraps `build.ps1` and writes a timestamped transcript to
`$env:PROJECT_PATH/logs/`.  Use it as the Task Scheduler action:

- **Program**: `powershell.exe`
- **Arguments**:
  ```
  -NonInteractive -ExecutionPolicy Bypass -File "C:\path\to\admin\win\build\task-build-log.ps1" -BuildType Release
  ```
- The task exit code mirrors the build result so the Scheduler can report
  success or failure.

---

## Migrating from client-building

| Old batch script | New PowerShell script |
|---|---|
| `init.bat` | `admin/win/build/init.ps1` |
| `build.bat [BuildType]` | `admin/win/build/build.ps1 -BuildType <type>` |
| `build-desktop.bat` | `admin/win/build/build-desktop.ps1` |
| `build-installer-collect.bat` | `admin/win/build/build-installer-collect.ps1` |
| `build-installer-msi.bat` | `admin/win/build/build-installer-msi.ps1` |
| `sign.bat <file>` | `admin/win/build/sign.ps1 -FilePath <file>` |
| `upload.bat <file>` | `admin/win/build/upload.ps1 -FilePath <file>` |
| `task-build-log.bat` | `admin/win/build/task-build-log.ps1` |
| `make-msi.bat` _(generated)_ | `make-msi.ps1` _(generated from `admin/win/msi/make-msi.ps1.in`)_ |

The NSIS installer workflow (`build-installer-exe.bat` / `nextcloud.nsi`) is not
carried forward; the MSI is the sole Windows installer format going forward.

All environment variable names are **unchanged**, so existing CI variable
configurations and `.env` files require no modification.
