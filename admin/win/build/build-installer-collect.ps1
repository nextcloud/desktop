#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Collects the runtime files required to package the Nextcloud Desktop client.
.DESCRIPTION
    For each architecture in BUILD_TARGETS this script:
      1. Stages all client binaries into a clean collect directory.
      2. Runs windeployqt to add the necessary Qt plugins and libraries.
      3. Copies Qt, OpenSSL, KArchive, VC Redistributable, and other dependencies.
      4. Optionally signs the key binaries with the configured certificate.
.PARAMETER BuildType
    CMake build type: Release, RelWithDebInfo (default), or Debug.
.EXAMPLE
    .\build-installer-collect.ps1 -BuildType Release
#>
param(
    [string]$BuildType = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

if ($BuildType) { $env:BUILD_TYPE = $BuildType }

Write-Host "`n***** Build started: build-installer-collect.ps1"
Write-Host "* BUILD_TYPE      = $env:BUILD_TYPE"
Write-Host "* BUILD_TARGETS   = $env:BUILD_TARGETS"
Write-Host "* USE_CODE_SIGNING= $env:USE_CODE_SIGNING"

$requiredVars = @('APP_NAME', 'APP_NAME_SANITIZED', 'PROJECT_PATH', 'BUILD_TYPE',
                  'BUILD_TARGETS', 'WIN_GIT_PATH', 'VCINSTALLDIR')
if ($env:USE_CODE_SIGNING -ne '0') {
    $requiredVars += @('APPLICATION_VENDOR', 'TAG_DESKTOP', 'APPLICATION_NAME',
                       'CERTIFICATE_FILENAME', 'CERTIFICATE_CSP',
                       'CERTIFICATE_KEY_CONTAINER_NAME', 'CERTIFICATE_PASSWORD',
                       'SIGN_FILE_DIGEST_ALG', 'SIGN_TIMESTAMP_URL',
                       'SIGN_TIMESTAMP_DIGEST_ALG')
}
Test-RequiredEnv @requiredVars

if ($env:TEST_RUN -eq '1') {
    Write-Host '** TEST RUN - exit.'
    exit 0
}

$targets = $env:BUILD_TARGETS -split '[, ]+' | Where-Object { $_ }

foreach ($arch in $targets) {
    Write-Host "`n**** Collecting installer files for $arch ($env:BUILD_TYPE)"

    Initialize-BuildEnvironment -BuildType $env:BUILD_TYPE `
                                -BuildArch $arch `
                                -BranchName $env:CRAFT_TAG_DESKTOP

    $dllSuffix       = if ($env:BUILD_TYPE -eq 'Debug') { 'd' } else { '' }
    $myRepo          = "$env:PROJECT_PATH/desktop"
    $myBuildPath     = "$myRepo/build"
    $myInstallPath   = "$env:PROJECT_PATH/install/$env:BUILD_TYPE/$arch"
    $myCollectPath   = "$env:PROJECT_PATH/collect/$env:BUILD_TYPE/$arch"
    $extraDeployPath = "$env:PROJECT_PATH/deploy-extra/$env:BUILD_TYPE/$arch"
    $appExe          = "$($env:APP_NAME_SANITIZED).exe"

    Write-Host "* APP_NAME_SANITIZED = $env:APP_NAME_SANITIZED"
    Write-Host "* MY_COLLECT_PATH    = $myCollectPath"
    Write-Host "* EXTRA_DEPLOY_PATH  = $extraDeployPath"
    Write-Host "* CRAFT_PATH         = $env:CRAFT_PATH"

    # ── Activate MSVC environment (needed for VC Redist path + signtool) ──────
    Write-BuildStep "Activating MSVC environment for $arch"
    Invoke-VsDevShell -Arch $arch

    # ── Prepare clean staging directory ──────────────────────────────────────
    Write-BuildStep 'Preparing staging directory'
    Remove-Item -Path "$myCollectPath\*" -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $myCollectPath -Force | Out-Null

    # ── Copy client binaries and resources ────────────────────────────────────
    Write-BuildStep 'Copying client files'
    Copy-Item -Path "$myInstallPath\i18n"  -Destination $myCollectPath -Recurse -Force
    Copy-Item -Path "$myInstallPath\bin\*" -Destination $myCollectPath -Recurse -Force
    Copy-Item -Path "$myInstallPath\config\$env:APP_NAME\sync-exclude.lst" `
              -Destination $myCollectPath -Force

    # Application icon (fall back to the generic installer icon)
    $icoSrc = "$myBuildPath\src\gui\$($env:APP_NAME_SANITIZED).ico"
    $icoDst = "$myCollectPath\$($env:APP_NAME_SANITIZED).ico"
    if (Test-Path $icoSrc) {
        Copy-Item -Path $icoSrc -Destination $icoDst -Force
    } else {
        Write-Host "  Icon not found at $icoSrc – using installer.ico as fallback"
        Copy-Item -Path "$myRepo\admin\win\nsi\installer.ico" -Destination $icoDst -Force
    }

    # ── windeployqt on the collected binaries ─────────────────────────────────
    Write-BuildStep 'Running windeployqt on staged binaries'
    Invoke-NativeCommand -Executable "$env:QT_BIN_PATH\windeployqt.exe" -ArgumentList @(
        '--compiler-runtime',
        '--qmldir', "$myRepo\src",
        '--release', '--force', '--verbose', '2',
        "$myCollectPath\$appExe",
        "$myCollectPath\$($env:APP_NAME_SANITIZED)_csync.dll",
        "$myCollectPath\$($env:APP_NAME_SANITIZED)cmd.exe",
        "$myCollectPath\$($env:APP_NAME_SANITIZED)sync.dll"
    ) -WorkingDirectory $myCollectPath

    # Qt bearer plugins are known to cause network issues on Windows – remove them
    Remove-Item -Path "$myCollectPath\bearer" -Recurse -Force -ErrorAction SilentlyContinue

    # ── Third-party libraries ─────────────────────────────────────────────────
    Write-BuildStep 'Copying third-party libraries'

    # freetype
    Copy-Item -Path "$env:CRAFT_PATH\bin\freetype$dllSuffix.dll" `
              -Destination $myCollectPath -Force

    # qt.conf to configure Qt paths for the deployed layout
    Copy-Item -Path "$myRepo\admin\win\nsi\qt.conf" -Destination $myCollectPath -Force

    # OpenSSL – discover the exact versioned filename at build time
    $libCrypto = Get-ChildItem "$env:CRAFT_PATH\bin\libcrypto-3*.dll" | Select-Object -First 1
    if (-not $libCrypto) { throw "libcrypto-3*.dll not found in $env:CRAFT_PATH\bin" }
    Copy-Item -Path $libCrypto.FullName -Destination $myCollectPath -Force
    Write-Host "* Copied $($libCrypto.Name)"

    $libSsl = Get-ChildItem "$env:CRAFT_PATH\bin\libssl-3*.dll" | Select-Object -First 1
    if (-not $libSsl) { throw "libssl-3*.dll not found in $env:CRAFT_PATH\bin" }
    Copy-Item -Path $libSsl.FullName -Destination $myCollectPath -Force
    Write-Host "* Copied $($libSsl.Name)"

    # KArchive and supporting libraries
    foreach ($pattern in @(
            "KF6Archive$dllSuffix.dll", "bz2$dllSuffix.dll",
            "liblzma$dllSuffix.dll", "zstd$dllSuffix.dll",
            "pcre2-16$dllSuffix.dll", "libpng16$dllSuffix.dll",
            "harfbuzz$dllSuffix.dll", "jpeg62$dllSuffix.dll"
        )) {
        $matches = Get-ChildItem "$env:CRAFT_PATH\bin\$pattern" -ErrorAction SilentlyContinue
        if ($matches) {
            $matches | ForEach-Object {
                Copy-Item -Path $_.FullName -Destination $myCollectPath -Force
                Write-Host "* Copied $($_.Name)"
            }
        } else {
            throw "Required library not found: $env:CRAFT_PATH\bin\$pattern"
        }
    }

    # Optional libraries – emit a warning when absent instead of failing
    foreach ($dll in @(
            "libp11.dll",
            "kdsingleapplication-qt6.dll",
            "zlib1$dllSuffix.dll",
            "brotlicommon.dll",
            "brotlidec.dll",
            "libsqlite.dll",
            "b2-1.dll"
        )) {
        $src = "$env:CRAFT_PATH\bin\$dll"
        if (Test-Path $src) {
            Copy-Item -Path $src -Destination $myCollectPath -Force
            Write-Host "* Copied $dll"
        } else {
            Write-Warning "Optional library not found (skipping): $src"
        }
    }

    # Qt OpenSSL TLS backend plugin
    $tlsDir   = "$myCollectPath\tls"
    New-Item -ItemType Directory -Path $tlsDir -Force | Out-Null
    $qosslSrc = "$env:CRAFT_PATH\plugins\tls\qopensslbackend.dll"
    if (Test-Path $qosslSrc) {
        Copy-Item -Path $qosslSrc -Destination $tlsDir -Force
        Write-Host '* Copied qopensslbackend.dll'
    } else {
        Write-Warning "qopensslbackend.dll not found: $qosslSrc"
    }

    # Optional extra deployment resources (branding assets, etc.)
    $extraFiles = Get-ChildItem "$extraDeployPath\*" -ErrorAction SilentlyContinue
    if ($extraFiles) {
        Write-Host "* Copying extra resources from $extraDeployPath"
        Copy-Item -Path "$extraDeployPath\*" -Destination $myCollectPath -Recurse -Force
    } else {
        Write-Host "* No extra resources at $extraDeployPath (skipping)"
    }

    # ── VC Redistributable ────────────────────────────────────────────────────
    Write-BuildStep 'Copying VC Redistributable'
    $redistArch = if ($arch -eq 'Win64') { 'x64' } else { 'x86' }
    Copy-Item -Path "$env:VCToolsRedistDir\$redistArch\Microsoft.VC143.CRT\*" `
              -Destination $myCollectPath -Force
    Copy-Item -Path "$env:VCToolsRedistDir\$redistArch\Microsoft.VC143.OpenMP\*" `
              -Destination $myCollectPath -Force
    # Remove any VC Redist installer executable dropped by windeployqt
    Remove-Item -Path "$myCollectPath\vc_redist*.exe" -Force -ErrorAction SilentlyContinue

    # ── Code signing ──────────────────────────────────────────────────────────
    if ($env:USE_CODE_SIGNING -ne '0') {
        Write-BuildStep 'Signing binaries'
        $signingTargets = @(
            'NCContextMenu.dll',
            'NCOverlays.dll',
            "$($env:APP_NAME_SANITIZED).exe",
            "$($env:APP_NAME_SANITIZED)cmd.exe",
            "$($env:APP_NAME_SANITIZED)sync.dll",
            "$($env:APP_NAME_SANITIZED)_csync.dll",
            "qt6keychain$dllSuffix.dll",
            $libCrypto.Name,
            $libSsl.Name,
            "zlib1$dllSuffix.dll"
        )
        foreach ($bin in $signingTargets) {
            $target = "$myCollectPath\$bin"
            if (Test-Path $target) {
                Invoke-Sign -FilePath $target
            } else {
                Write-Warning "Binary not found for signing (skipping): $target"
            }
        }
    } else {
        Write-Host '** Skipping signing (USE_CODE_SIGNING=0)'
    }

    Write-Host "`n*** Finished collect: $env:BUILD_TYPE $arch"
}

Write-Host "`n***** Build finished: build-installer-collect.ps1"
