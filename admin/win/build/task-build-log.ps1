#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Runs the full build and captures all output to a timestamped log file.
.DESCRIPTION
    Intended for use with the Windows Task Scheduler for automatic (e.g. daily)
    builds.  Invokes build.ps1, writes a transcript to
    PROJECT_PATH/logs/last-build-<timestamp>-<BuildType>.log, and exits with the
    build exit code so the Task Scheduler can report success or failure correctly.
.PARAMETER BuildType
    CMake build type: Release, RelWithDebInfo (default), or Debug.
.EXAMPLE
    .\task-build-log.ps1 -BuildType Release
#>
param(
    [string]$BuildType = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

if ($BuildType) { $env:BUILD_TYPE = $BuildType }

$timestamp = Get-Date -Format 'yyyy-MM-dd-HH-mm-ss'
$logDir    = "$env:PROJECT_PATH/logs"
$logFile   = "$logDir/last-build-$timestamp-$env:BUILD_TYPE.log"

New-Item -ItemType Directory -Path $logDir -Force | Out-Null

Write-Host "--- START: task-build-log.ps1  $(Get-Date)  -->  $logFile"

$exitCode = 0
try {
    Start-Transcript -Path $logFile -Append
    & "$PSScriptRoot\build.ps1" -BuildType $env:BUILD_TYPE
    $exitCode = if ($LASTEXITCODE) { $LASTEXITCODE } else { 0 }
} catch {
    Write-Error "Build failed: $_"
    $exitCode = 1
} finally {
    Stop-Transcript -ErrorAction SilentlyContinue
}

Write-Host "--- END:   task-build-log.ps1  $(Get-Date)  (exit=$exitCode)"
exit $exitCode
