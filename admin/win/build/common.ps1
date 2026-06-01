# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Shared helper functions used by all Windows build scripts.
# Dot-source this file to load them:  . "$PSScriptRoot\common.ps1"

Set-StrictMode -Version Latest

# Sets an environment variable only when it is not already set to a non-empty value.
function Set-DefaultEnv {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][AllowEmptyString()][string]$Value
    )
    if ([string]::IsNullOrEmpty([System.Environment]::GetEnvironmentVariable($Name))) {
        Set-Item -Path "Env:$Name" -Value $Value
    }
}

# Validates that each named environment variable is set and non-empty.
# Writes an error for every missing variable and exits with code 1.
function Test-RequiredEnv {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory, ValueFromRemainingArguments)]
        [string[]]$Names
    )
    $allOk = $true
    foreach ($name in $Names) {
        if ([string]::IsNullOrEmpty([System.Environment]::GetEnvironmentVariable($name))) {
            Write-Host "Missing required environment variable: $name" -ForegroundColor Red
            $allOk = $false
        }
    }
    if (-not $allOk) { exit 1 }
}

# Runs a native executable and throws a terminating error on non-zero exit.
function Invoke-NativeCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Executable,
        [string[]]$ArgumentList = @(),
        [string]$WorkingDirectory = ''
    )
    $prevLocation = Get-Location
    try {
        if ($WorkingDirectory -ne '') { Set-Location $WorkingDirectory }
        & $Executable @ArgumentList
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed (exit $LASTEXITCODE): $Executable $($ArgumentList -join ' ')"
        }
    } finally {
        Set-Location $prevLocation
    }
}

# Like Invoke-NativeCommand but returns captured stdout as a string.
function Invoke-NativeCommandOutput {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Executable,
        [string[]]$ArgumentList = @(),
        [string]$WorkingDirectory = ''
    )
    $prevLocation = Get-Location
    try {
        if ($WorkingDirectory -ne '') { Set-Location $WorkingDirectory }
        $output = & $Executable @ArgumentList
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed (exit $LASTEXITCODE): $Executable $($ArgumentList -join ' ')"
        }
        return $output
    } finally {
        Set-Location $prevLocation
    }
}

# Sets BUILD_ARCH, CMAKE_GENERATOR, CRAFT_PATH, QT_PATH, QT_BIN_PATH, QT_PREFIX,
# and prepends the Craft bin directory to PATH.
# Equivalent to common.inc.bat from nextcloud/client-building.
function Initialize-BuildEnvironment {
    [CmdletBinding()]
    param(
        [string]$BuildType  = '',
        [string]$BuildArch  = '',
        [string]$BranchName = ''
    )
    if ($BuildType  -ne '') { $env:BUILD_TYPE  = $BuildType  }
    if ($BuildArch  -ne '') { $env:BUILD_ARCH  = $BuildArch  }
    if ($BranchName -ne '') { $env:BRANCH_NAME = $BranchName }

    $env:CMAKE_GENERATOR = 'Ninja'

    switch ($env:BUILD_ARCH) {
        'Win32' {
            $craftPath       = 'c:\Craft32'
            $env:CRAFT_PATH  = $craftPath
            $env:QT_PATH     = $craftPath
            $env:QT_BIN_PATH = "$craftPath\bin"
            $env:QT_PREFIX   = $craftPath
            $env:PATH        = "$craftPath\bin;$craftPath\dev-utils\bin;$env:PATH"
        }
        default {
            # Win64 is the standard target architecture
            $branch          = if ($env:BRANCH_NAME) { $env:BRANCH_NAME } else { 'master' }
            $craftPath       = "c:\Nextcloud\$branch\windows-msvc2022_64-cl"
            $env:CRAFT_PATH  = $craftPath
            $env:QT_PATH     = $craftPath
            $env:QT_BIN_PATH = "$craftPath\bin"
            $env:QT_PREFIX   = $craftPath
            $env:PATH        = "$craftPath\bin;$craftPath\dev-utils\bin;$env:PATH"
        }
    }
}

# Activates the MSVC build environment for the given architecture by running
# vcvarsall.bat and importing all resulting environment variables into the
# current PowerShell session.
function Invoke-VsDevShell {
    [CmdletBinding()]
    param([string]$Arch = 'Win64')

    $vcvarsAll = Join-Path $env:VCINSTALLDIR 'Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvarsAll)) {
        throw "vcvarsall.bat not found at: $vcvarsAll"
    }
    $batArch  = if ($Arch -eq 'Win32') { 'amd64_x86' } else { 'x64' }
    $envLines = cmd.exe /c "`"$vcvarsAll`" $batArch 1>nul 2>nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "vcvarsall.bat failed for arch $batArch (exit $LASTEXITCODE)"
    }
    foreach ($line in $envLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}

# Signs a single file using signtool.exe with the configured certificate.
# Skips silently when USE_CODE_SIGNING is '0'.
function Invoke-Sign {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$FilePath)

    if ($env:USE_CODE_SIGNING -eq '0') {
        Write-Host "** Skipping signing (USE_CODE_SIGNING=0): $FilePath"
        return
    }

    Test-RequiredEnv 'APPLICATION_VENDOR', 'TAG_DESKTOP', 'APPLICATION_NAME',
                     'CERTIFICATE_FILENAME', 'CERTIFICATE_CSP',
                     'CERTIFICATE_KEY_CONTAINER_NAME', 'CERTIFICATE_PASSWORD',
                     'SIGN_FILE_DIGEST_ALG', 'SIGN_TIMESTAMP_URL', 'SIGN_TIMESTAMP_DIGEST_ALG'

    $signtool = $env:SIGNTOOL
    if (-not $signtool) {
        $signtool = (Get-Command 'signtool.exe' -ErrorAction SilentlyContinue)?.Source
    }
    if (-not $signtool) {
        throw 'signtool.exe not found in PATH. Run from a VS Developer shell or set the SIGNTOOL environment variable.'
    }

    $keyContainer = "[{{$env:CERTIFICATE_PASSWORD}}]=$env:CERTIFICATE_KEY_CONTAINER_NAME"
    Invoke-NativeCommand -Executable $signtool -ArgumentList @(
        'sign', '/debug', '/v',
        '/d', "$env:APPLICATION_NAME $env:TAG_DESKTOP",
        '/tr', $env:SIGN_TIMESTAMP_URL,
        '/td', $env:SIGN_TIMESTAMP_DIGEST_ALG,
        '/fd', $env:SIGN_FILE_DIGEST_ALG,
        '/f', $env:CERTIFICATE_FILENAME,
        '/csp', $env:CERTIFICATE_CSP,
        '/kc', $keyContainer,
        $FilePath
    )
}

# Uploads a file to the configured SFTP server via scp.
# Skips silently when UPLOAD_BUILD is '0'.
function Invoke-Upload {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$FilePath)

    if ($env:UPLOAD_BUILD -eq '0') {
        Write-Host '** Skipping upload (UPLOAD_BUILD=0)'
        return
    }

    Test-RequiredEnv 'SFTP_PATH', 'SFTP_SERVER', 'SFTP_USER'

    $sshKey   = Join-Path $env:USERPROFILE '.ssh\id_rsa'
    $filename = Split-Path $FilePath -Leaf
    Invoke-NativeCommand -Executable 'scp' -ArgumentList @(
        '-i', $sshKey,
        $FilePath,
        "$env:SFTP_USER@$env:SFTP_SERVER`:$env:SFTP_PATH/$filename"
    )

    if ($env:UPLOAD_DELETE -eq '1') {
        Remove-Item -Path $FilePath -Force
    }
}

# Prints a formatted stage banner.
function Write-BuildStep {
    param([Parameter(Mandatory)][string]$Message)
    Write-Host ''
    Write-Host ":: $Message" -ForegroundColor Cyan
}
