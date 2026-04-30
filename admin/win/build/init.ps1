#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Initialises the build workspace by cloning required repositories and
    creating the directory structure expected by the build scripts.
.DESCRIPTION
    Run this script once before the first build.
    It clones qtkeychain, zlib, and the Nextcloud Desktop client into
    PROJECT_PATH and creates the deploy-extra and installer output directories.
.EXAMPLE
    .\init.ps1
#>
param()

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

Write-Host "`n***** Init started: init.ps1"
Write-Host "* BUILD_TARGETS         = $env:BUILD_TARGETS"
Write-Host "* PROJECT_PATH          = $env:PROJECT_PATH"
Write-Host "* INSTALLER_OUTPUT_PATH = $env:INSTALLER_OUTPUT_PATH"
Write-Host "* TAG_DESKTOP           = $env:TAG_DESKTOP"

Test-RequiredEnv 'PROJECT_PATH', 'BUILD_TARGETS'

if ($env:TEST_RUN -eq '1') {
    Write-Host '** TEST RUN - exit.'
    exit 0
}

# ── Deploy-extra directories (one per build type × architecture) ──────────────
$targets = $env:BUILD_TARGETS -split '[, ]+' | Where-Object { $_ }
foreach ($buildTypeDir in @('Debug', 'Release')) {
    foreach ($arch in $targets) {
        $dir = "$env:PROJECT_PATH/deploy-extra/$buildTypeDir/$arch"
        Write-Host "* Creating directory: $dir"
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

# ── Installer output directory ────────────────────────────────────────────────
Write-Host "* Creating installer output directory: $env:INSTALLER_OUTPUT_PATH"
New-Item -ItemType Directory -Path $env:INSTALLER_OUTPUT_PATH -Force | Out-Null

# ── Clone qtkeychain ──────────────────────────────────────────────────────────
Write-BuildStep 'Cloning qtkeychain'
Invoke-NativeCommand -Executable 'git' -ArgumentList @(
    'clone', 'https://github.com/frankosterfeld/qtkeychain'
) -WorkingDirectory $env:PROJECT_PATH
New-Item -ItemType Directory -Path "$env:PROJECT_PATH/qtkeychain/build" -Force | Out-Null

# ── Clone zlib ────────────────────────────────────────────────────────────────
Write-BuildStep 'Cloning zlib'
Invoke-NativeCommand -Executable 'git' -ArgumentList @(
    'clone', 'https://github.com/madler/zlib.git'
) -WorkingDirectory $env:PROJECT_PATH
New-Item -ItemType Directory -Path "$env:PROJECT_PATH/zlib/build" -Force | Out-Null

# ── Clone the Nextcloud Desktop client ────────────────────────────────────────
Write-BuildStep "Cloning desktop client at tag $env:TAG_DESKTOP"
$cloneArgs = @('clone')
# Use a shallow clone unless CUSTOMIZATION_SERVICE requires full history
if ($env:CUSTOMIZATION_SERVICE -ne '1') { $cloneArgs += '--depth=1' }
$cloneArgs += "--branch=$env:TAG_DESKTOP"
$cloneArgs += 'https://github.com/nextcloud/client'
$cloneArgs += "$env:PROJECT_PATH/desktop"
Invoke-NativeCommand -Executable 'git' -ArgumentList $cloneArgs

Write-Host "`n***** Init finished: init.ps1"
