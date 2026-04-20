#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Uploads a build artefact to the configured SFTP server.
.DESCRIPTION
    Thin entry point for Invoke-Upload defined in common.ps1.
    Skips silently when UPLOAD_BUILD is '0'.
.PARAMETER FilePath
    Absolute path to the file to upload (e.g. the produced .msi).
.EXAMPLE
    .\upload.ps1 -FilePath 'C:\build\daily\Nextcloud-6.0.0-x64.msi'
#>
param(
    [Parameter(Mandatory)][string]$FilePath
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\defaults.ps1"

Write-Host "`n*** Upload file: $FilePath"
Invoke-Upload -FilePath $FilePath
Write-Host "*** Finished upload: $FilePath"
