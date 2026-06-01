#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Signs a single binary file using the configured code-signing certificate.
.DESCRIPTION
    Thin entry point for Invoke-Sign defined in common.ps1.
    Skips silently when USE_CODE_SIGNING is '0'.
.PARAMETER FilePath
    Absolute path to the file to sign (e.g. an .exe, .dll, or .msi).
.EXAMPLE
    .\sign.ps1 -FilePath 'C:\build\collect\Release\Win64\Nextcloud.exe'
#>
param(
    [Parameter(Mandatory)][string]$FilePath
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

Write-Host "`n*** Sign file: $FilePath"
Invoke-Sign -FilePath $FilePath
Write-Host "*** Finished signing: $FilePath"
