#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Orchestrates the full Windows build: desktop compile, file collection, and
    MSI installer creation.
.DESCRIPTION
    Runs build-desktop.ps1, build-installer-collect.ps1, and
    build-installer-msi.ps1 in sequence.  Any stage failure stops the build and
    propagates a non-zero exit code.
.PARAMETER BuildType
    CMake build type: Release, RelWithDebInfo (default), or Debug.
.EXAMPLE
    .\build.ps1 -BuildType Release
.EXAMPLE
    $env:USE_CODE_SIGNING = '0'; $env:UPLOAD_BUILD = '0'; .\build.ps1 -BuildType Release
#>
param(
    [string]$BuildType = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

if ($BuildType) { $env:BUILD_TYPE = $BuildType }

Write-Host "`n***** Build started: build.ps1"
Write-Host "* BUILD_TYPE    = $env:BUILD_TYPE"
Write-Host "* BUILD_TARGETS = $env:BUILD_TARGETS"

Test-RequiredEnv 'PROJECT_PATH', 'BUILD_TYPE', 'BUILD_TARGETS',
                 'Png2Ico_EXECUTABLE', 'VS_VERSION', 'VCINSTALLDIR', 'WIN_GIT_PATH'

# ── 1. Build desktop client ───────────────────────────────────────────────────
Write-BuildStep 'Stage 1: build desktop'
& "$PSScriptRoot\build-desktop.ps1" -BuildType $env:BUILD_TYPE
if ($LASTEXITCODE) { exit $LASTEXITCODE }

# ── 2. Collect runtime files ─────────────────────────────────────────────────
Write-BuildStep 'Stage 2: collect installer files'
& "$PSScriptRoot\build-installer-collect.ps1" -BuildType $env:BUILD_TYPE
if ($LASTEXITCODE) { exit $LASTEXITCODE }

# ── 3. Build MSI installer ────────────────────────────────────────────────────
if ($env:BUILD_INSTALLER_MSI -ne '0') {
    Write-BuildStep 'Stage 3: build MSI installer'
    & "$PSScriptRoot\build-installer-msi.ps1" -BuildType $env:BUILD_TYPE
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
} else {
    Write-Host '** Skipping MSI installer (BUILD_INSTALLER_MSI=0)'
}

Write-Host "`n***** Build finished: build.ps1"
