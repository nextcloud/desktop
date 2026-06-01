#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Builds the Nextcloud Desktop client for each configured architecture.
.DESCRIPTION
    For every architecture listed in BUILD_TARGETS this script:
      1. Activates the correct MSVC build environment (via vcvarsall.bat).
      2. Optionally clones / updates the desktop source repository.
      3. Runs CMake configure, build, and install.
      4. Deploys Qt libraries with windeployqt.
.PARAMETER BuildType
    CMake build type: Release, RelWithDebInfo (default), or Debug.
.EXAMPLE
    .\build-desktop.ps1 -BuildType Release
#>
param(
    [string]$BuildType = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

if ($BuildType) { $env:BUILD_TYPE = $BuildType }

Write-Host "`n***** Build started: build-desktop.ps1"
Write-Host "* BUILD_TYPE    = $env:BUILD_TYPE"
Write-Host "* BUILD_TARGETS = $env:BUILD_TARGETS"

Test-RequiredEnv 'APP_NAME', 'PROJECT_PATH', 'BUILD_TYPE', 'BUILD_TARGETS',
                 'VCINSTALLDIR', 'Png2Ico_EXECUTABLE', 'WIX_SDK_PATH',
                 'BUILD_DATE', 'BUILD_UPDATER', 'TAG_DESKTOP'

if ($env:TEST_RUN -eq '1') {
    Write-Host '** TEST RUN - exit.'
    exit 0
}

$targets = $env:BUILD_TARGETS -split '[, ]+' | Where-Object { $_ }

foreach ($arch in $targets) {
    Write-Host "`n**** Building desktop for $arch ($env:BUILD_TYPE)"

    Initialize-BuildEnvironment -BuildType $env:BUILD_TYPE `
                                -BuildArch $arch `
                                -BranchName $env:CRAFT_TAG_DESKTOP

    $tag            = if ($env:TAG_DESKTOP)    { $env:TAG_DESKTOP }    else { 'master' }
    $versionSuffix  = if ($env:VERSION_SUFFIX) { $env:VERSION_SUFFIX } else { '' }
    # Pass an explicit empty string to CMake so the variable is defined but blank
    $cmakeVerSuffix = if ($versionSuffix) { "-DMIRALL_VERSION_SUFFIX=$versionSuffix" } `
                                          else { '-DMIRALL_VERSION_SUFFIX=' }

    $myRepo         = "$env:PROJECT_PATH/desktop"
    $myBuildPath    = "$myRepo/build"
    $myInstallPath  = "$env:PROJECT_PATH/install/$env:BUILD_TYPE/$arch"
    $myQtDeployPath = "$myInstallPath/qt-libs"

    Write-Host "* APP_NAME        = $env:APP_NAME"
    Write-Host "* CRAFT_PATH      = $env:CRAFT_PATH"
    Write-Host "* QT_PREFIX       = $env:QT_PREFIX"
    Write-Host "* VCINSTALLDIR    = $env:VCINSTALLDIR"
    Write-Host "* MY_REPO         = $myRepo"
    Write-Host "* MY_BUILD_PATH   = $myBuildPath"
    Write-Host "* MY_INSTALL_PATH = $myInstallPath"
    Write-Host "* TAG             = $tag"
    Write-Host "* BUILD_DATE      = $env:BUILD_DATE"

    # ── Activate MSVC build environment ──────────────────────────────────────
    Write-BuildStep "Activating MSVC environment for $arch"
    Invoke-VsDevShell -Arch $arch

    # ── Clean previous output ─────────────────────────────────────────────────
    Write-BuildStep 'Cleaning previous output directories'
    Remove-Item -Path $myInstallPath  -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $myQtDeployPath -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $myBuildPath    -Recurse -Force -ErrorAction SilentlyContinue

    # ── Clone / update source ─────────────────────────────────────────────────
    if ($env:PULL_DESKTOP -eq '1' -or $env:CHECKOUT_DESKTOP -eq '1') {
        Write-BuildStep "Cloning desktop client at tag $tag"
        Remove-Item -Path $myRepo -Recurse -Force -ErrorAction SilentlyContinue
        Invoke-NativeCommand -Executable 'git' -ArgumentList @(
            'clone', '--depth=1', "--branch=$tag",
            'https://github.com/nextcloud/client', $myRepo
        )
    }

    # ── Create build directory ─────────────────────────────────────────────────
    New-Item -ItemType Directory -Path $myBuildPath -Force | Out-Null

    # ── Capture git revision ──────────────────────────────────────────────────
    $gitRevision = (Invoke-NativeCommandOutput -Executable 'git' `
                        -ArgumentList @('-C', $myRepo, 'rev-parse', 'HEAD')).Trim()
    Write-Host "* GIT_REVISION = $gitRevision"

    # ── CMake configure ───────────────────────────────────────────────────────
    Write-BuildStep 'CMake configure'
    $cmakeArgs = @(
        "-G$env:CMAKE_GENERATOR", '..',
        $cmakeVerSuffix,
        '-DBUILD_TESTING=OFF',
        '-DWITH_CRASHREPORTER=OFF',
        "-DBUILD_UPDATER=$env:BUILD_UPDATER",
        "-DBUILD_WIN_MSI=$env:BUILD_INSTALLER_MSI",
        "-DMIRALL_VERSION_BUILD=$env:BUILD_DATE",
        "-DCMAKE_INSTALL_PREFIX=$myInstallPath",
        "-DCMAKE_BUILD_TYPE=$env:BUILD_TYPE",
        "-DCMAKE_PREFIX_PATH=$env:CRAFT_PATH",
        "-DPng2Ico_EXECUTABLE=$env:Png2Ico_EXECUTABLE",
        "-DWIX_SDK_PATH=$env:WIX_SDK_PATH"
    )
    if ($env:CMAKE_EXTRA_FLAGS_DESKTOP) {
        $cmakeArgs += $env:CMAKE_EXTRA_FLAGS_DESKTOP -split '\s+'
    }
    Invoke-NativeCommand -Executable 'cmake' -ArgumentList $cmakeArgs `
                         -WorkingDirectory $myBuildPath

    # ── CMake build and install ───────────────────────────────────────────────
    Write-BuildStep 'CMake build and install'
    Invoke-NativeCommand -Executable 'cmake' -ArgumentList @(
        '--build', '.', '--config', $env:BUILD_TYPE, '--target', 'install'
    ) -WorkingDirectory $myBuildPath

    # ── windeployqt ───────────────────────────────────────────────────────────
    Write-BuildStep 'Deploying Qt libraries'
    $wdqtType = if ($env:BUILD_TYPE -eq 'Debug') { 'debug' } else { 'release' }
    New-Item -ItemType Directory -Path $myQtDeployPath -Force | Out-Null
    Invoke-NativeCommand -Executable "$env:QT_BIN_PATH\windeployqt.exe" -ArgumentList @(
        "--$wdqtType",
        '--compiler-runtime',
        "$myInstallPath\bin\$env:APP_NAME.exe",
        '--dir', $myQtDeployPath,
        '--qmldir', "$myRepo\src\gui",
        '-websockets'
    )

    Write-Host "`n*** Finished desktop $env:BUILD_TYPE $arch (GIT_REVISION=$gitRevision)"
}

Write-Host "`n***** Build finished: build-desktop.ps1"
