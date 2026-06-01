# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build defaults: sets the environment variables consumed by all build scripts.
# Dot-source this file:  . "$PSScriptRoot\defaults.ps1"
#
# Each variable is applied only when it is not already present in the environment,
# so any value set by the caller before dot-sourcing is preserved.

. "$PSScriptRoot\common.ps1"

# ── Branding ──────────────────────────────────────────────────────────────────
Set-DefaultEnv 'APP_NAME'           'Nextcloud'
Set-DefaultEnv 'APP_NAME_SANITIZED' 'Nextcloud'
Set-DefaultEnv 'APPLICATION_NAME'   'Nextcloud Files Client'

# ── Build type / targets ──────────────────────────────────────────────────────
Set-DefaultEnv 'USE_BRANDING'  '0'
Set-DefaultEnv 'BUILD_TYPE'    'RelWithDebInfo'
Set-DefaultEnv 'BUILD_TARGETS' 'Win64'

# ── Paths ─────────────────────────────────────────────────────────────────────
# PROJECT_PATH is the working directory for the build (separate from the
# source tree).  Repos are cloned and artefacts are written here.
Set-DefaultEnv 'PROJECT_PATH'       'c:/Nextcloud/client-building'
Set-DefaultEnv 'Png2Ico_EXECUTABLE' 'c:/Nextcloud/tools/png2ico.exe'

# ── Visual Studio ─────────────────────────────────────────────────────────────
Set-DefaultEnv 'VS_VERSION' '2022'
if ([string]::IsNullOrEmpty($env:VCINSTALLDIR)) {
    $env:VCINSTALLDIR = switch ($env:VS_VERSION) {
        '2017'  { 'C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC' }
        '2019'  { 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC' }
        default { 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC' }
    }
}

# ── Git ───────────────────────────────────────────────────────────────────────
Set-DefaultEnv 'WIN_GIT_PATH' 'C:\Program Files\Git'

# ── Dry-run / test mode ───────────────────────────────────────────────────────
# When set to '1', scripts print their variables and exit without building.
Set-DefaultEnv 'TEST_RUN' '0'

# ── Build date and version suffix ─────────────────────────────────────────────
Set-DefaultEnv 'BUILD_DATE'      (Get-Date -Format 'yyyyMMdd')
Set-DefaultEnv 'VERSION_SUFFIX'  ''

# ── Desktop git tags ──────────────────────────────────────────────────────────
Set-DefaultEnv 'TAG_DESKTOP'       'master'
Set-DefaultEnv 'CRAFT_TAG_DESKTOP' 'master'

# ── Git pull / checkout behaviour ─────────────────────────────────────────────
Set-DefaultEnv 'PULL_DESKTOP'     '1'
Set-DefaultEnv 'CHECKOUT_DESKTOP' '1'

# Branding builds are typically not re-fetched automatically
if ($env:USE_BRANDING -eq '1') {
    $env:PULL_DESKTOP     = '0'
    $env:CHECKOUT_DESKTOP = '0'
}

# ── Updater ────────────────────────────────────────────────────────────────────
Set-DefaultEnv 'BUILD_UPDATER' 'OFF'

# ── MSI installer ─────────────────────────────────────────────────────────────
Set-DefaultEnv 'BUILD_INSTALLER_MSI'   '1'
Set-DefaultEnv 'INSTALLER_OUTPUT_PATH' "$env:PROJECT_PATH/daily"

# ── Code signing ──────────────────────────────────────────────────────────────
Set-DefaultEnv 'USE_CODE_SIGNING'               '1'
Set-DefaultEnv 'APPLICATION_VENDOR'             'Nextcloud GmbH'
Set-DefaultEnv 'CERTIFICATE_FILENAME'           ''
Set-DefaultEnv 'CERTIFICATE_CSP'                ''
Set-DefaultEnv 'CERTIFICATE_KEY_CONTAINER_NAME' ''
Set-DefaultEnv 'CERTIFICATE_PASSWORD'           ''
Set-DefaultEnv 'SIGN_FILE_DIGEST_ALG'           'sha256'
Set-DefaultEnv 'SIGN_TIMESTAMP_URL'             'http://timestamp.digicert.com'
Set-DefaultEnv 'SIGN_TIMESTAMP_DIGEST_ALG'      'sha256'

# ── Upload ─────────────────────────────────────────────────────────────────────
Set-DefaultEnv 'UPLOAD_BUILD'  '1'
Set-DefaultEnv 'UPLOAD_DELETE' '0'
Set-DefaultEnv 'SFTP_PATH'     '/var/www/html/desktop/daily/Windows'
Set-DefaultEnv 'SFTP_SERVER'   ''
Set-DefaultEnv 'SFTP_USER'     ''

# ── CMake extra flags ──────────────────────────────────────────────────────────
Set-DefaultEnv 'CMAKE_EXTRA_FLAGS_DESKTOP' ''

# ── WiX Toolset ───────────────────────────────────────────────────────────────
Set-DefaultEnv 'WIX'         'C:/Program Files (x86)/WiX Toolset v3.14'
Set-DefaultEnv 'WIX_SDK_PATH' 'C:/Program Files (x86)/WiX Toolset v3.14/SDK/VS2017'
