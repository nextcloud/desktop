#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Builds the WiX MSI installer for each configured architecture.
.DESCRIPTION
    For each architecture in BUILD_TARGETS this script:
      1. Activates the MSVC build environment (needed for signtool and WiX scripts).
      2. Optionally signs the MSI helper DLL.
      3. Detects the WiX SDK VBScripts for multi-language support.
      4. Runs the CMake-generated make-msi.ps1 to produce the MSI file.
      5. Optionally signs the MSI.
      6. Moves the MSI to INSTALLER_OUTPUT_PATH.
      7. Optionally uploads the MSI via scp.
.PARAMETER BuildType
    CMake build type: Release, RelWithDebInfo (default), or Debug.
.EXAMPLE
    .\build-installer-msi.ps1 -BuildType Release
#>
param(
    [string]$BuildType = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

if ($BuildType) { $env:BUILD_TYPE = $BuildType }

Write-Host "`n***** Build started: build-installer-msi.ps1"
Write-Host "* BUILD_TYPE          = $env:BUILD_TYPE"
Write-Host "* BUILD_TARGETS       = $env:BUILD_TARGETS"
Write-Host "* INSTALLER_OUTPUT_PATH = $env:INSTALLER_OUTPUT_PATH"
Write-Host "* USE_CODE_SIGNING    = $env:USE_CODE_SIGNING"
Write-Host "* UPLOAD_BUILD        = $env:UPLOAD_BUILD"

$requiredVars = @('PROJECT_PATH', 'INSTALLER_OUTPUT_PATH', 'BUILD_TYPE',
                  'BUILD_TARGETS', 'BUILD_DATE', 'WIX', 'VCINSTALLDIR')
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

New-Item -ItemType Directory -Path $env:INSTALLER_OUTPUT_PATH -Force | Out-Null

$targets = $env:BUILD_TARGETS -split '[, ]+' | Where-Object { $_ }

foreach ($arch in $targets) {
    Write-Host "`n**** Building MSI for $arch ($env:BUILD_TYPE)"

    Initialize-BuildEnvironment -BuildType $env:BUILD_TYPE `
                                -BuildArch $arch `
                                -BranchName $env:CRAFT_TAG_DESKTOP

    $myInstallPath = "$env:PROJECT_PATH/install/$env:BUILD_TYPE/$arch"
    $myCollectPath = "$env:PROJECT_PATH/collect/$env:BUILD_TYPE/$arch"
    $myMsiPath     = "$myInstallPath/msi"

    Write-Host "* MY_INSTALL_PATH = $myInstallPath"
    Write-Host "* MY_COLLECT_PATH = $myCollectPath"
    Write-Host "* MY_MSI_PATH     = $myMsiPath"
    Write-Host "* WIX             = $env:WIX"

    # ── Activate MSVC environment (required for signtool and WiSubStg/WiLangId) ─
    Write-BuildStep "Activating MSVC environment for $arch"
    Invoke-VsDevShell -Arch $arch

    # ── Optionally sign the MSI helper DLL before building the MSI ────────────
    if ($env:USE_CODE_SIGNING -ne '0') {
        $bitness   = if ($arch -eq 'Win64') { '64' } else { '32' }
        $helperDll = "$myMsiPath\NCMsiHelper$bitness.dll"
        if (Test-Path $helperDll) {
            Write-BuildStep "Signing NCMsiHelper$bitness.dll"
            Invoke-Sign -FilePath $helperDll
        }
    }

    # ── Locate optional WiX SDK VBScripts for multi-language MSI builds ───────
    # WiSubStg.vbs embeds language transforms; WiLangId.vbs updates Package/@Language.
    # Both scripts ship with the Windows Installer SDK and are added to PATH by vcvarsall.
    $wisubstg = (Get-Command 'WiSubStg.vbs' -ErrorAction SilentlyContinue)?.Source
    $wilangid  = (Get-Command 'WiLangId.vbs' -ErrorAction SilentlyContinue)?.Source

    if ($wisubstg) {
        $env:WISUBSTG = $wisubstg
        Write-Host "* WiSubStg.vbs found: $wisubstg"
    } else {
        Write-Host '* WiSubStg.vbs not found – MSI will be English-only'
        $env:WISUBSTG = ''
    }
    if ($wilangid) {
        $env:WILANGID = $wilangid
        Write-Host "* WiLangId.vbs found: $wilangid"
    } else {
        Write-Host '* WiLangId.vbs not found – Package language IDs will not be updated'
        $env:WILANGID = ''
    }

    # ── Run the CMake-generated make-msi.ps1 ─────────────────────────────────
    Write-BuildStep 'Running make-msi.ps1'
    $makeMsiScript = Join-Path $myMsiPath 'make-msi.ps1'
    if (-not (Test-Path $makeMsiScript)) {
        throw "make-msi.ps1 not found at $makeMsiScript. Run cmake --build --target install first."
    }
    # make-msi.ps1 expects to run from the directory that contains the WXS files
    Push-Location $myMsiPath
    try {
        & $makeMsiScript -HarvestAppDir $myCollectPath
        if ($LASTEXITCODE) { throw "make-msi.ps1 failed (exit $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }

    # ── Locate the produced MSI ───────────────────────────────────────────────
    $msiFile = Get-ChildItem "$myMsiPath\*.msi" | Select-Object -First 1
    if (-not $msiFile) { throw "No .msi file found in $myMsiPath after make-msi.ps1" }
    Write-Host "* MSI produced: $($msiFile.Name)"

    # ── Sign the MSI ──────────────────────────────────────────────────────────
    if ($env:USE_CODE_SIGNING -ne '0') {
        Write-BuildStep "Signing $($msiFile.Name)"
        Invoke-Sign -FilePath $msiFile.FullName
    }

    # ── Move to installer output directory ────────────────────────────────────
    Write-BuildStep "Moving MSI to $env:INSTALLER_OUTPUT_PATH"
    Move-Item -Path $msiFile.FullName -Destination $env:INSTALLER_OUTPUT_PATH -Force

    # ── Upload ────────────────────────────────────────────────────────────────
    $outputMsi = Join-Path $env:INSTALLER_OUTPUT_PATH $msiFile.Name
    Invoke-Upload -FilePath $outputMsi

    Write-Host "`n*** Finished MSI: $env:BUILD_TYPE $arch ($($msiFile.Name))"
}

Write-Host "`n***** Build finished: build-installer-msi.ps1"
